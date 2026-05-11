#!/bin/bash
# run_exp.sh — Ossim experiment automation runner (Slice A).
#
# Single entrypoint that drives one experiment end-to-end:
#   preflight → build_deploy → run_collect → cleanup.
#
# Spec: clawspace/experiment-automation-design-draft-2026-05-11.md
# Scope: Slice A — `exp_s1` physical-only single-run happy path.
#
# Usage:
#   ./run_exp.sh <exp_name> [--target <host>] [--from-phase <phase>]
#                [--run-dir <path>] [--break-stale-lock [--force]]
#                [--with-trace <profile>]
#
# --target falls back to $OSSIM_REMOTE_TARGET when not given.

set -u
set -o pipefail

# --------------------------------------------------------------------------
# Repo + experiment paths

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXPERIMENTS_ROOT="$REPO_ROOT/experiments"
RUNS_ROOT="$REPO_ROOT/runs"

# --------------------------------------------------------------------------
# Globals populated by parse_args + setup
EXP_NAME=""
TARGET="${OSSIM_REMOTE_TARGET:-}"
FROM_PHASE="preflight"
RUN_DIR=""
BREAK_STALE_LOCK=0
FORCE=0
WITH_TRACE=""

DESCRIPTOR=""
RUNNER_LOG=""
STATE_JSON=""
MANIFEST_JSON=""
HEARTBEAT_PID=""

# Cleanup state — populated by preflight + run phases so cleanup knows what
# was started by *this* run vs. what existed beforehand.
declare -A CLEANUP_STATE=(
    [lock_acquired]=0
    [lock_path]=/tmp/ossim_exp.lock
    [ossim_module_loaded_prior]=unknown
    [ossim_subsystem_enabled_prior]=unknown
    [run_started]=0
)

# --------------------------------------------------------------------------
# Logging

log() { printf '[%s] %s\n' "$(date -u +%H:%M:%S)" "$*"; }
log_to() { printf '[%s] %s\n' "$(date -u +%H:%M:%S)" "$*" | tee -a "${RUNNER_LOG:-/dev/null}"; }
die() { log "FATAL: $*" >&2; exit 1; }

print_help() {
    cat <<'EOF'
Usage: run_exp.sh <exp_name> [options]

Options:
  --target <host>          remote target host (falls back to $OSSIM_REMOTE_TARGET)
  --from-phase <phase>     resume from one of: preflight, build_deploy, run_collect
  --run-dir <path>         explicit run directory (default: auto-allocated under runs/)
  --break-stale-lock       attempt to break a stale lock during preflight
  --force                  non-interactive: skip the --break-stale-lock confirmation prompt
  --with-trace <profile>   enable heavy collectors (comma-separated, e.g. ftrace,perf)
  -h, --help               show this help

Slice A scope: only `exp_s1` with PHASE=physical is fully supported.
See clawspace/experiment-automation-design-draft-2026-05-11.md.
EOF
}

# --------------------------------------------------------------------------
# Argument parsing

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --target)             TARGET="$2"; shift 2 ;;
            --from-phase)         FROM_PHASE="$2"; shift 2 ;;
            --run-dir)            RUN_DIR="$2"; shift 2 ;;
            --break-stale-lock)   BREAK_STALE_LOCK=1; shift ;;
            --force)              FORCE=1; shift ;;
            --with-trace)         WITH_TRACE="$2"; shift 2 ;;
            -h|--help)            print_help; exit 0 ;;
            -*)                   die "unknown option: $1" ;;
            *)
                if [[ -z "$EXP_NAME" ]]; then
                    EXP_NAME="$1"; shift
                else
                    die "extra positional argument: $1"
                fi
                ;;
        esac
    done

    [[ -n "$EXP_NAME" ]]              || die "missing <exp_name>"
    [[ -n "$TARGET" ]]                || die "missing --target and \$OSSIM_REMOTE_TARGET is unset"
    [[ "$FROM_PHASE" =~ ^(preflight|build_deploy|run_collect)$ ]] \
        || die "invalid --from-phase: $FROM_PHASE"

    DESCRIPTOR="$EXPERIMENTS_ROOT/$EXP_NAME/experiment.yaml"
    [[ -f "$DESCRIPTOR" ]] || die "experiment descriptor not found: $DESCRIPTOR"
}

# --------------------------------------------------------------------------
# YAML loader — uses python3 to convert YAML to JSON so jq can query it.

descriptor_json() {
    python3 - "$DESCRIPTOR" <<'PY'
import json, sys
import yaml
with open(sys.argv[1]) as f:
    print(json.dumps(yaml.safe_load(f)))
PY
}

desc_get() {
    # desc_get '.field.path'
    descriptor_json | jq -r "$1"
}

# --------------------------------------------------------------------------
# Run directory allocation

allocate_run_dir() {
    if [[ -n "$RUN_DIR" ]]; then
        mkdir -p "$RUN_DIR"
        return
    fi
    local meta_sha utc base counter
    meta_sha="$(git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    utc="$(date -u +%Y-%m-%dT%H-%M-%SZ)"
    base="$RUNS_ROOT/$EXP_NAME"
    mkdir -p "$base"
    counter=1
    while [[ -e "$base/$utc-$meta_sha-$(printf '%02d' "$counter")" ]]; do
        counter=$((counter + 1))
    done
    RUN_DIR="$base/$utc-$meta_sha-$(printf '%02d' "$counter")"
    mkdir -p "$RUN_DIR"
}

setup_paths() {
    mkdir -p "$RUN_DIR/logs" "$RUN_DIR/config" "$RUN_DIR/results" "$RUN_DIR/artifacts" "$RUN_DIR/work"
    RUNNER_LOG="$RUN_DIR/logs/runner.log"
    STATE_JSON="$RUN_DIR/state.json"
    MANIFEST_JSON="$RUN_DIR/manifest.json"
    : > "$RUNNER_LOG"
}

# --------------------------------------------------------------------------
# State + manifest

# state_set <phase> <classification> [extras_json]
state_set() {
    local phase="$1" classification="$2" extras="${3:-{\}}"
    local now; now="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    local cur; cur="$(cat "$STATE_JSON" 2>/dev/null || echo '{}')"
    printf '%s' "$cur" | jq --arg p "$phase" --arg c "$classification" \
        --arg t "$now" --argjson e "$extras" \
        '. + {current_phase: $p, current_classification: $c, updated: $t}
         | .phases[$p] = ((.phases[$p] // {}) + {classification: $c, updated: $t} + $e)' \
        > "$STATE_JSON.tmp" && mv "$STATE_JSON.tmp" "$STATE_JSON"
}

manifest_init() {
    local now; now="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    local meta_sha kernel_sha libossim_sha qemu_sha
    meta_sha="$(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
    kernel_sha="$(git -C "$REPO_ROOT/kernel" rev-parse HEAD 2>/dev/null || echo unknown)"
    libossim_sha="$(git -C "$REPO_ROOT/libossim" rev-parse HEAD 2>/dev/null || echo unknown)"
    qemu_sha="$(git -C "$REPO_ROOT/qemu" rev-parse HEAD 2>/dev/null || echo unknown)"

    jq -n --arg name "$EXP_NAME" --arg run_dir "$RUN_DIR" \
          --arg target "$TARGET" --arg start "$now" \
          --arg meta "$meta_sha" --arg k "$kernel_sha" \
          --arg l "$libossim_sha" --arg q "$qemu_sha" \
          --arg trace "$WITH_TRACE" \
        '{
            schema_version: "1.0",
            experiment_name: $name,
            run_dir: $run_dir,
            target: $target,
            start_ts: $start,
            git_shas: {
                meta_repo: $meta,
                kernel: $k,
                libossim: $l,
                qemu: $q
            },
            with_trace: $trace,
            phases: [],
            cleanup: [],
            workload_provenance: {},
            final: null,
            end_ts: null
         }' > "$MANIFEST_JSON"
}

# manifest_finalize <phase> <classification>
manifest_finalize() {
    local phase="$1" classification="$2"
    local now; now="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    jq --arg p "$phase" --arg c "$classification" --arg t "$now" \
        '.final = {phase: $p, classification: $c} | .end_ts = $t' \
        "$MANIFEST_JSON" > "$MANIFEST_JSON.tmp" && mv "$MANIFEST_JSON.tmp" "$MANIFEST_JSON"
}

# manifest_record_phase <phase> <classification> <duration_s>
manifest_record_phase() {
    local phase="$1" classification="$2" duration="$3"
    local now; now="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    jq --arg p "$phase" --arg c "$classification" \
       --argjson dur "$duration" --arg t "$now" \
       '.phases += [{phase: $p, classification: $c, duration_sec: $dur, end_ts: $t}]' \
       "$MANIFEST_JSON" > "$MANIFEST_JSON.tmp" && mv "$MANIFEST_JSON.tmp" "$MANIFEST_JSON"
}

# manifest_record_cleanup <step> <status> [detail]
manifest_record_cleanup() {
    local step="$1" status="$2" detail="${3:-}"
    jq --arg s "$step" --arg st "$status" --arg d "$detail" \
       '.cleanup += [{step: $s, status: $st, detail: $d}]' \
       "$MANIFEST_JSON" > "$MANIFEST_JSON.tmp" && mv "$MANIFEST_JSON.tmp" "$MANIFEST_JSON"
}

# manifest_set_provenance — writes the workload_provenance object
manifest_set_provenance() {
    local prov_json="$1"
    jq --argjson p "$prov_json" '.workload_provenance = $p' \
       "$MANIFEST_JSON" > "$MANIFEST_JSON.tmp" && mv "$MANIFEST_JSON.tmp" "$MANIFEST_JSON"
}

# --------------------------------------------------------------------------
# Remote lock with heartbeat. Target-side file at /tmp/ossim_exp.lock.
# Heartbeat: 10s interval, 30s TTL.
#
# Lock format (single line of JSON):
#   {"owner":"yiliangw","pid":12345,"host":"sandbox","start_ts":"...","heartbeat_ts":"..."}

LOCK_HEARTBEAT_SEC=10
LOCK_STALE_TTL_SEC=30

# remote runs a command on $TARGET. Stdout/stderr returned to caller.
remote() {
    ssh "$TARGET" -- "$@"
}

acquire_remote_lock() {
    local lock="${CLEANUP_STATE[lock_path]}"
    local owner pid host start_ts hb_ts now stale heartbeat_age
    owner="${USER:-unknown}"
    pid="$$"
    host="$(hostname)"
    start_ts="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

    # Probe existing lock
    local existing
    existing="$(remote "cat $lock 2>/dev/null || echo ''")"

    if [[ -n "$existing" ]]; then
        local existing_hb existing_pid
        existing_hb="$(printf '%s' "$existing" | jq -r '.heartbeat_ts // empty' 2>/dev/null || true)"
        existing_pid="$(printf '%s' "$existing" | jq -r '.pid // empty' 2>/dev/null || true)"
        now="$(date -u +%s)"
        local hb_epoch
        if [[ -n "$existing_hb" ]]; then
            hb_epoch="$(date -u -d "$existing_hb" +%s 2>/dev/null || echo 0)"
        else
            hb_epoch=0
        fi
        heartbeat_age=$(( now - hb_epoch ))

        # Owner alive on target?
        local alive=0
        if [[ -n "$existing_pid" ]]; then
            if remote "kill -0 $existing_pid 2>/dev/null"; then alive=1; fi
        fi

        if (( heartbeat_age <= LOCK_STALE_TTL_SEC )) || (( alive == 1 )); then
            die "remote lock $lock is held: $existing (heartbeat_age=${heartbeat_age}s, alive=$alive)"
        fi

        # Stale candidate.
        log_to "lock at $lock looks stale: $existing (heartbeat_age=${heartbeat_age}s, alive=$alive)"
        if (( BREAK_STALE_LOCK == 0 )); then
            die "lock is stale but --break-stale-lock not given; aborting"
        fi
        if (( FORCE == 0 )) && [[ -t 0 ]]; then
            read -p "Break stale lock $lock? [y/N] " ans
            [[ "$ans" == "y" || "$ans" == "Y" ]] || die "user declined to break stale lock"
        elif (( FORCE == 0 )); then
            die "--break-stale-lock requires --force in non-interactive mode"
        fi
        log_to "breaking stale lock $lock (--break-stale-lock --force)"
        manifest_record_cleanup "stale_lock_break" success "broke $existing"
    fi

    hb_ts="$start_ts"
    local lock_json
    lock_json="$(jq -n --arg o "$owner" --arg p "$pid" --arg h "$host" \
                  --arg s "$start_ts" --arg hb "$hb_ts" \
                  '{owner:$o, pid:($p|tonumber), host:$h, start_ts:$s, heartbeat_ts:$hb}')"
    # Atomic create
    if ! remote "[ ! -e $lock ] && printf '%s\n' $(printf '%q' "$lock_json") > $lock"; then
        die "failed to atomically create remote lock $lock"
    fi
    CLEANUP_STATE[lock_acquired]=1
    log_to "acquired remote lock $lock (owner=$owner pid=$pid)"

    # Heartbeat in background
    (
        while sleep "$LOCK_HEARTBEAT_SEC"; do
            local nhb; nhb="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
            local nj
            nj="$(jq -n --arg o "$owner" --arg p "$pid" --arg h "$host" \
                       --arg s "$start_ts" --arg hb "$nhb" \
                       '{owner:$o, pid:($p|tonumber), host:$h, start_ts:$s, heartbeat_ts:$hb}')"
            remote "printf '%s\n' $(printf '%q' "$nj") > $lock" 2>/dev/null || break
        done
    ) &
    HEARTBEAT_PID=$!
}

release_remote_lock() {
    if (( CLEANUP_STATE[lock_acquired] == 1 )); then
        if [[ -n "$HEARTBEAT_PID" ]]; then
            kill "$HEARTBEAT_PID" 2>/dev/null || true
            wait "$HEARTBEAT_PID" 2>/dev/null || true
            HEARTBEAT_PID=""
        fi
        if remote "rm -f ${CLEANUP_STATE[lock_path]}"; then
            manifest_record_cleanup "release_lock" success ""
        else
            manifest_record_cleanup "release_lock" failed "rm -f returned non-zero"
        fi
        CLEANUP_STATE[lock_acquired]=0
    fi
}

# --------------------------------------------------------------------------
# Preflight

phase_preflight() {
    log_to "=== preflight ==="
    local t0; t0=$(date +%s)
    state_set preflight in_progress

    # Reachability
    if ! ssh -o BatchMode=yes -o ConnectTimeout=10 "$TARGET" true 2>&1 | tee -a "$RUN_DIR/logs/preflight.log"; then
        state_set preflight phase_fail '{"detail":"ssh unreachable"}'
        manifest_record_phase preflight phase_fail $(( $(date +%s) - t0 ))
        die "target $TARGET unreachable"
    fi

    # Required tools on target
    local missing=""
    for cmd in make bash python3 git; do
        if ! remote "command -v $cmd >/dev/null 2>&1"; then
            missing+="$cmd "
        fi
    done
    [[ -z "$missing" ]] || die "target missing tools: $missing"

    # Disk space (need ~10G free under /home/$user)
    local free_gb
    free_gb="$(remote "df -BG --output=avail \$HOME | tail -n1 | tr -d 'G '")"
    if [[ -n "$free_gb" && "$free_gb" -lt 5 ]]; then
        die "target has only ${free_gb}G free under \$HOME; need ≥ 5G"
    fi

    # Workspace cleanliness check (warn only — caller decided what to ship)
    for r in kernel libossim qemu; do
        if ! git -C "$REPO_ROOT/$r" diff --quiet 2>/dev/null; then
            log_to "warning: $REPO_ROOT/$r has uncommitted changes"
        fi
    done

    # Record ossim prior state (so cleanup knows whether to unload module)
    local mod_loaded sub_enabled
    mod_loaded="$(remote "lsmod 2>/dev/null | grep -c '^ossim ' || true")"
    sub_enabled="$(remote "cat /sys/kernel/ossim/enabled 2>/dev/null || echo 0")"
    CLEANUP_STATE[ossim_module_loaded_prior]="$mod_loaded"
    CLEANUP_STATE[ossim_subsystem_enabled_prior]="$sub_enabled"

    # Lock
    acquire_remote_lock

    # Workload provenance
    local prov="{}"
    local files; files=$(desc_get '.workload.provenance_files[]?')
    while IFS= read -r f; do
        [[ -z "$f" ]] && continue
        local path="$REPO_ROOT/$f"
        if [[ -f "$path" ]]; then
            local h; h="$(sha256sum "$path" | awk '{print $1}')"
            prov="$(printf '%s' "$prov" | jq --arg k "$f" --arg v "$h" '. + {($k): $v}')"
        fi
    done <<<"$files"
    manifest_set_provenance "$prov"

    # Snapshot resolved descriptor + env
    cp "$DESCRIPTOR" "$RUN_DIR/config/experiment.resolved.yaml"
    env > "$RUN_DIR/config/env.txt"

    state_set preflight success
    manifest_record_phase preflight success $(( $(date +%s) - t0 ))
    log_to "preflight OK"
}

# --------------------------------------------------------------------------
# Build / deploy — invokes existing make remote-* targets.

phase_build_deploy() {
    log_to "=== build_deploy ==="
    local t0; t0=$(date +%s)
    state_set build_deploy in_progress

    local logf="$RUN_DIR/logs/build_deploy.log"
    export OSSIM_REMOTE_TARGET="$TARGET"

    for tgt in remote-vng-kernel remote-libossim remote-qemu; do
        log_to "running: make $tgt (OSSIM_REMOTE_TARGET=$TARGET)"
        if ! make -C "$REPO_ROOT" "$tgt" >>"$logf" 2>&1; then
            log_to "make $tgt failed; tail -20 of $logf:"
            tail -20 "$logf" | tee -a "$RUNNER_LOG"
            state_set build_deploy phase_fail '{"target":"'"$tgt"'"}'
            manifest_record_phase build_deploy phase_fail $(( $(date +%s) - t0 ))
            die "build/deploy failed at $tgt"
        fi
    done

    state_set build_deploy success
    manifest_record_phase build_deploy success $(( $(date +%s) - t0 ))
    log_to "build_deploy OK"
}

# --------------------------------------------------------------------------
# Run + collect

phase_run_collect() {
    log_to "=== run_collect ==="
    local t0; t0=$(date +%s)
    state_set run_collect in_progress
    CLEANUP_STATE[run_started]=1

    local logf="$RUN_DIR/logs/run_collect.log"
    local timeout_s; timeout_s="$(desc_get '.timeout_policy.run_collect_sec // 120')"

    # Resolve workload command + env
    local cmd; cmd="$(desc_get '.workload.command')"
    local env_kv
    env_kv="$(desc_get '.workload.env | to_entries | map("\(.key)=\(.value|tostring)") | join(" ")')"

    # Run remotely. OSSIM_OUT_DIR is set so exp_s1.sh's $EXP_OUT_BASE
    # resolves under $RUN_DIR/work/ (and we sync results back below).
    # Target the work/ subdir so the runner-owned state.json /
    # manifest.json / logs/ / config/ siblings don't get rsync'd into
    # work/ as a side-effect of the NFS-shared path.
    local remote_repo="/nfs/ossim-workspace/ossim"
    local remote_outdir="$remote_repo/runs/$EXP_NAME/$(basename "$RUN_DIR")/work"
    log_to "remote OSSIM_OUT_DIR: $remote_outdir"

    if ! ssh "$TARGET" "cd $remote_repo && \
            mkdir -p $remote_outdir && \
            export OSSIM_OUT_DIR=$remote_outdir && \
            export WORKLOADS_ROOT=$remote_repo/workloads && \
            timeout ${timeout_s}s env $env_kv $cmd" \
            >>"$logf" 2>&1; then
        log_to "workload failed; tail -20 of $logf:"
        tail -20 "$logf" | tee -a "$RUNNER_LOG"
        state_set run_collect phase_fail '{"detail":"workload non-zero exit"}'
        manifest_record_phase run_collect phase_fail $(( $(date +%s) - t0 ))
        return 1
    fi

    # Sync results back. Use rsync via ssh.
    log_to "collecting results from $remote_outdir"
    if ! rsync -aq "$TARGET:$remote_outdir/" "$RUN_DIR/work/"; then
        state_set run_collect phase_fail '{"detail":"rsync results failed"}'
        manifest_record_phase run_collect phase_fail $(( $(date +%s) - t0 ))
        return 1
    fi

    # Validate success criteria
    local result_glob; result_glob="$(desc_get '.success_criteria.result_glob')"
    local result_path="$RUN_DIR/$result_glob"
    if [[ ! -s "$result_path" ]]; then
        state_set run_collect phase_fail '{"detail":"result missing","path":"'"$result_path"'"}'
        manifest_record_phase run_collect phase_fail $(( $(date +%s) - t0 ))
        die "expected result not found: $result_path"
    fi
    if ! jq -e . "$result_path" >/dev/null 2>&1; then
        state_set run_collect phase_fail '{"detail":"result not valid JSON"}'
        manifest_record_phase run_collect phase_fail $(( $(date +%s) - t0 ))
        die "result is not valid JSON: $result_path"
    fi
    local bench
    bench="$(jq -r '.benchmark // "?"' "$result_path")"
    local expected_bench
    expected_bench="$(desc_get '.success_criteria.benchmark_eq')"
    if [[ "$bench" != "$expected_bench" ]]; then
        state_set run_collect phase_fail '{"detail":"benchmark mismatch","expected":"'"$expected_bench"'","got":"'"$bench"'"}'
        manifest_record_phase run_collect phase_fail $(( $(date +%s) - t0 ))
        die "result .benchmark expected '$expected_bench', got '$bench'"
    fi
    local samples_min
    samples_min="$(desc_get '.success_criteria.samples_min // 1')"
    local samples_n
    samples_n="$(jq -r '.samples | length' "$result_path")"
    if (( samples_n < samples_min )); then
        state_set run_collect phase_fail '{"detail":"samples too few","got":'"$samples_n"',"min":'"$samples_min"'}'
        manifest_record_phase run_collect phase_fail $(( $(date +%s) - t0 ))
        die "result .samples has $samples_n entries (need ≥ $samples_min)"
    fi

    # Copy primary result into results/ for easy discovery
    cp "$result_path" "$RUN_DIR/results/$(basename "$result_path")"

    # Artifacts: dmesg tail + journal excerpt
    remote "dmesg --ctime 2>/dev/null | tail -200" > "$RUN_DIR/artifacts/dmesg.tail.log" 2>/dev/null || true
    remote "journalctl -n 200 --no-pager 2>/dev/null || true" > "$RUN_DIR/artifacts/journal.tail.log" 2>/dev/null || true

    state_set run_collect success
    manifest_record_phase run_collect success $(( $(date +%s) - t0 ))
    log_to "run_collect OK ($result_path)"
}

# --------------------------------------------------------------------------
# Cleanup trap (inverse-startup order; honors prior-state guard).

cleanup() {
    local exit_code=$?
    log_to "=== cleanup ==="

    # Only attempt the workload-side teardown if we actually started a run.
    if (( CLEANUP_STATE[run_started] == 1 )); then
        # For the physical phase no qemu/ossimd/vng are started. The hooks
        # below remain as stubs so the manifest cleanup section is complete
        # and so Slice B can grow them.
        manifest_record_cleanup "stop_qemu" skipped "no qemu launched for physical phase"
        manifest_record_cleanup "stop_ossimd" skipped "no ossimd launched for physical phase"
        manifest_record_cleanup "ossimctl_disable" skipped "physical phase does not touch ossim"
        manifest_record_cleanup "unload_ossim_module" skipped \
            "module-prior=${CLEANUP_STATE[ossim_module_loaded_prior]}; not loaded by this run"
        manifest_record_cleanup "stop_vng" skipped "no vng instance started by this run"
    fi

    release_remote_lock

    # Final manifest
    local final_phase final_class
    final_phase="$(jq -r '.current_phase // "preflight"' "$STATE_JSON" 2>/dev/null)"
    final_class="$(jq -r '.current_classification // "phase_fail"' "$STATE_JSON" 2>/dev/null)"
    if (( exit_code == 0 )) && [[ "$final_class" != "success" ]]; then
        final_class="success"
    elif (( exit_code != 0 )) && [[ "$final_class" == "success" ]]; then
        final_class="phase_fail"
    fi
    manifest_finalize "$final_phase" "$final_class"

    log_to "exit=$exit_code final=$final_phase/$final_class run=$RUN_DIR"
}

# --------------------------------------------------------------------------
# Main

main() {
    parse_args "$@"
    allocate_run_dir
    setup_paths
    log_to "run_dir: $RUN_DIR"
    log_to "exp: $EXP_NAME  target: $TARGET"
    log_to "from_phase: $FROM_PHASE  with_trace: ${WITH_TRACE:-(none)}"

    # Validate python yaml available before doing anything else
    python3 -c 'import yaml' 2>/dev/null || die "python3 PyYAML is required (pip install pyyaml)"

    manifest_init
    state_set bootstrap success '{"started":"'"$(date -u +%Y-%m-%dT%H:%M:%SZ)"'"}'

    trap cleanup EXIT

    case "$FROM_PHASE" in
        preflight)
            phase_preflight
            phase_build_deploy
            phase_run_collect
            ;;
        build_deploy)
            log_to "skipping preflight (--from-phase=build_deploy)"
            # We still need a lock; acquire it without re-running provenance.
            acquire_remote_lock
            phase_build_deploy
            phase_run_collect
            ;;
        run_collect)
            log_to "skipping preflight + build_deploy (--from-phase=run_collect)"
            acquire_remote_lock
            phase_run_collect
            ;;
    esac
}

main "$@"
