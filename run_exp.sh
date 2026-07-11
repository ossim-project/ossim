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

    local run_id; run_id="$(basename "$RUN_DIR")"
    jq -n --arg name "$EXP_NAME" --arg run_id "$run_id" --arg run_dir "$RUN_DIR" \
          --arg target "$TARGET" --arg start "$now" \
          --arg meta "$meta_sha" --arg k "$kernel_sha" \
          --arg l "$libossim_sha" --arg q "$qemu_sha" \
          --arg trace "$WITH_TRACE" \
        '{
            schema_version: "1.0",
            experiment_name: $name,
            run_id: $run_id,
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

    # Mode-aware target list. multi_vm_barrier benchmarks need ossim live
    # on the host kernel, so build + install the local (host) ossim kernel
    # — vng is intentionally NOT used for benchmarking (nested KVM would
    # break sync semantics). native (physical) needs only userspace bits.
    local mode; mode="$(desc_get '.mode // "native"')"
    local targets
    case "$mode" in
        multi_vm_barrier)
            # `install-*` variants: build targets alone (e.g. `qemu`,
            # `libossim`) only compile into $build/; we need the binaries
            # actually deployed under $prefix/ so qemu launches and
            # ossimctl uses the just-built code. `install-local-kernel`
            # only refreshes /boot/vmlinuz-* + initrd (modules stay
            # stale to save time); pair with the kexec preflight gate.
            targets=(target-local-kernel target-install-local-kernel \
                     target-install-libossim target-install-qemu)
            ;;
        *)
            targets=(target-install-libossim target-install-qemu)
            ;;
    esac

    for tgt in "${targets[@]}"; do
        log_to "running: make $tgt (OSSIM_TARGET_LOGIN=$TARGET)"
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
    local mode; mode="$(desc_get '.mode // "native"')"
    case "$mode" in
        native)            phase_run_collect_native ;;
        multi_vm_barrier)  phase_run_collect_multi_vm_barrier ;;
        *)                 die "unknown experiment mode: $mode" ;;
    esac
}

# --------------------------------------------------------------------------
# Run-collect dispatch — native (single-process, host-side workload).

phase_run_collect_native() {
    log_to "=== run_collect (mode=native) ==="
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
# Multi-VM-barrier dispatch — for clean.ossim and similar multi-guest
# workloads. Driver-side flow:
#   1) reset ossim subsystem
#   2) for each instance N: clear stale barrier/result/ready files,
#      write start_bench.sh into instance-N/output/, launch QEMU in
#      background (the guest auto-runs /out/start_bench.sh via the
#      autologin .bash_profile hook installed by
#      workloads/disks/microbench/install.sh).
#   3) wait for /out/ready_vm-N marker from each instance
#   4) ossimctl enable-sync (if descriptor.ossim_sync == true)
#   5) touch /out/start (release barrier) for each instance
#   6) wait for the result JSON in each instance's /out
#   7) rsync results into $RUN_DIR/work/, validate, finish

phase_run_collect_multi_vm_barrier() {
    log_to "=== run_collect (mode=multi_vm_barrier) ==="
    local t0; t0=$(date +%s)
    state_set run_collect in_progress
    CLEANUP_STATE[run_started]=1
    CLEANUP_STATE[multi_vm_barrier_active]=1

    local timeout_s; timeout_s="$(desc_get '.timeout_policy.run_collect_sec // 600')"
    local n_vms; n_vms="$(desc_get '.multi_vm.n_vms // 2')"
    local result_filename; result_filename="$(desc_get '.multi_vm.result_filename // "cpu_clean_ossim.json"')"
    local ready_timeout_s; ready_timeout_s="$(desc_get '.multi_vm.ready_timeout_sec // 300')"
    local ossim_sync; ossim_sync="$(desc_get '.multi_vm.ossim_sync // false')"
    local vtime_epoch_ns; vtime_epoch_ns="$(desc_get '.multi_vm.vtime_epoch_ns // 10000000')"

    local remote_repo="/nfs/ossim-workspace/ossim"
    local prefix="/home/yiliangw/ossim.local/prefix"
    local out_base="/home/yiliangw/ossim.local/out/workloads/disks/microbench"

    # Sanity: confirm the autologin auto-run hook is baked into the
    # microbench image. If not, the bench will never start inside the
    # guest and the ready-marker wait will time out. Fast-fail with a
    # clear pointer at the image-rebuild step.
    log_to "preflight: check guest image has autologin auto-run hook"
    if ! ssh "$TARGET" "sudo virt-cat -a '$out_base/disk.qcow2' /home/ossim/.bash_profile 2>/dev/null | grep -q 'OSSIM_AUTORUN_DONE'" 2>/dev/null; then
        log_to "warning: cannot verify image auto-run hook via virt-cat (libguestfs absent or image-side check failed). Proceeding; if ready markers never appear, rebuild the image: make -C workloads dimg-microbench"
    fi

    # Kexec gate: the multi_vm_barrier mode needs /dev/ossim live on the
    # host kernel. If lab is currently on a non-ossim kernel, switch to
    # the locally-installed ossim kernel via `make kexec-local-kernel`.
    # kexec drops the SSH session at systemctl kexec time (expected); the
    # runner waits for SSH to come back, then re-verifies /dev/ossim.
    log_to "preflight: verify running kernel exposes /dev/ossim"
    local host_kernel; host_kernel="$(remote 'uname -r' 2>/dev/null || echo unknown)"
    log_to "  current kernel on $TARGET: $host_kernel"
    if ! remote "[ -c /dev/ossim ]"; then
        log_to "  /dev/ossim absent on $host_kernel; invoking kexec to local ossim kernel"
        local kexec_t0; kexec_t0=$(date +%s)
        # `make target-kexec-local-kernel` runs `ssh -t lab '... && systemctl kexec'`;
        # the ssh exits non-zero when the host kexecs out from under it.
        # We don't treat that exit as a failure — we verify by reconnect.
        make -C "$REPO_ROOT" target-kexec-local-kernel >>"$RUN_DIR/logs/kexec.log" 2>&1 || true
        log_to "  kexec initiated; waiting for SSH on $TARGET to come back (cap 180s)"
        local kexec_deadline=$(( $(date +%s) + 180 ))
        local ssh_back=0
        while (( $(date +%s) < kexec_deadline )); do
            if ssh -o BatchMode=yes -o ConnectTimeout=5 -o StrictHostKeyChecking=accept-new \
                   "$TARGET" true 2>/dev/null; then
                ssh_back=1; break
            fi
            sleep 3
        done
        local kexec_elapsed=$(( $(date +%s) - kexec_t0 ))
        if (( ssh_back == 0 )); then
            state_set run_collect phase_fail '{"detail":"kexec: SSH did not return within 180s"}'
            manifest_record_phase run_collect phase_fail $(( $(date +%s) - t0 ))
            die "lab did not come back after kexec within 180s (elapsed ${kexec_elapsed}s)"
        fi
        log_to "  SSH back after ${kexec_elapsed}s; re-verifying /dev/ossim"
        host_kernel="$(remote 'uname -r' 2>/dev/null || echo unknown)"
        echo "{\"kexec_reconnect_sec\": $kexec_elapsed, \"kernel_after\": \"$host_kernel\"}" \
            > "$RUN_DIR/artifacts/kexec_timing.json"
        if ! remote "[ -c /dev/ossim ]"; then
            state_set run_collect phase_fail '{"detail":"/dev/ossim still absent after kexec","kernel_after":"'"$host_kernel"'"}'
            manifest_record_phase run_collect phase_fail $(( $(date +%s) - t0 ))
            die "/dev/ossim still absent after kexec into local kernel (now on $host_kernel)"
        fi
        log_to "  kexec OK; now on $host_kernel with /dev/ossim live"
    fi

    # Reset ossim subsystem
    log_to "ossimctl: disable + enable"
    remote "$prefix/bin/ossimctl disable >/dev/null 2>&1; true"
    remote "$prefix/bin/ossimctl enable" || die "ossimctl enable failed"

    # For each instance: stage start_bench.sh + launch QEMU
    local instances=()
    for ((n=0; n<n_vms; n++)); do instances+=("$n"); done

    local cpusets_csv; cpusets_csv="$(desc_get '.multi_vm.cpusets // [] | join(",")')"
    IFS=',' read -ra CPUSETS <<<"$cpusets_csv"

    local time_s; time_s="$(desc_get '.multi_vm.time_s // 10')"
    local sample_ms; sample_ms="$(desc_get '.multi_vm.sample_ms // 100')"
    local inner; inner="$(desc_get '.multi_vm.inner // 100000')"
    local label; label="$(desc_get '.multi_vm.bench_label // "clean_ossim"')"

    local launch_pids=""
    for n in "${instances[@]}"; do
        local cpuset="${CPUSETS[$n]:-}"
        local inst_out="$out_base/instance-$n/output"

        # Stage: clear stale state, write start_bench.sh
        remote "mkdir -p $inst_out && rm -f $inst_out/start $inst_out/ready_vm-$n $inst_out/$result_filename"
        remote "cat > $inst_out/start_bench.sh <<'SCRIPT'
#!/bin/bash
# Auto-generated by run_exp.sh for instance N=$n.
exec env \\
    BENCH=/input/bench_cpu.py \\
    OUTPUT=/out/$result_filename \\
    VM_LABEL=vm-$n \\
    OSSIM_MODE=$([[ "$ossim_sync" == "true" ]] && echo "sync" || echo "disabled") \\
    EXP_LABEL=$label \\
    HOST_CPUSET=\"$cpuset\" \\
    ARGS=\"--mode loop --time $time_s --sample-ms $sample_ms --inner $inner\" \\
    /input/run_in_vm.sh
SCRIPT
chmod +x $inst_out/start_bench.sh"

        local logf="$RUN_DIR/logs/vm-$n.log"
        log_to "launching VM N=$n cpuset='$cpuset'"
        # Background ssh locally rather than detaching the remote
        # command. Tried remote nohup+setsid and `( cmd & )` subshells;
        # both leak ssh's stdout/stderr fds to the backgrounded child
        # and ssh blocks until the child exits. By keeping ssh in the
        # foreground (relative to its own process) but backgrounding it
        # on the local side, the SSH connection stays alive for the
        # qemu's lifetime — and cleanup gets a free win: killing the
        # local ssh sends SIGHUP through the channel, terminating the
        # remote make+qemu naturally.
        ssh -n "$TARGET" "cd $remote_repo/workloads && \
            ${cpuset:+QEMU_CPUSET=$cpuset} \
            make qemu-microbench-instance N=$n ${cpuset:+QEMU_CPUSET=$cpuset} \
            >/tmp/qemu-microbench-$n.log 2>&1" \
            </dev/null >>"$logf" 2>&1 &
        local launch_ssh_pid=$!
        CLEANUP_STATE[ssh_launch_pids]+="$launch_ssh_pid "
        log_to "  vm-$n launched (local ssh pid=$launch_ssh_pid)"
        launch_pids+="$n "
    done
    CLEANUP_STATE[multi_vm_instances]="${instances[*]}"

    # Wait for ready markers — guests now boot freely (no first-entry
    # gate after the qemu sync-gate removal), autologin fires, the
    # in-VM run_in_vm.sh writes /out/ready_vm-N and parks on the
    # barrier. By the time every marker appears, pvclock is active on
    # every vCPU thread, which is what the kernel's OSSIM_ENABLE_SYNC
    # needs to resolve kvm_vcpu_handles.
    log_to "waiting for ready markers (timeout ${ready_timeout_s}s)"
    local deadline=$(( $(date +%s) + ready_timeout_s ))
    for n in "${instances[@]}"; do
        local marker="$out_base/instance-$n/output/ready_vm-$n"
        while ! remote "[ -f $marker ]"; do
            if (( $(date +%s) >= deadline )); then
                state_set run_collect timeout '{"detail":"ready marker timeout","vm":'"$n"'}'
                manifest_record_phase run_collect timeout $(( $(date +%s) - t0 ))
                die "VM N=$n did not signal ready within ${ready_timeout_s}s; check $RUN_DIR/logs/vm-$n.log and /tmp/qemu-microbench-$n.log on $TARGET"
            fi
            sleep 2
        done
        log_to "  vm-$n ready"
    done

    # Enable sync now that every guest has reached the barrier. Single
    # call, no retry needed — pvclock is active on every vCPU by the
    # time the marker is written.
    if [[ "$ossim_sync" == "true" ]]; then
        log_to "ossimctl: enable-sync vtime_epoch=$vtime_epoch_ns"
        remote "$prefix/bin/ossimctl enable-sync $vtime_epoch_ns" \
            || die "ossimctl enable-sync failed"
    fi

    # Release barriers
    log_to "releasing barriers"
    for n in "${instances[@]}"; do
        remote ": > $out_base/instance-$n/output/start"
    done

    # Wait for results
    log_to "waiting for results (timeout ${timeout_s}s)"
    deadline=$(( $(date +%s) + timeout_s ))
    for n in "${instances[@]}"; do
        local result="$out_base/instance-$n/output/$result_filename"
        while ! remote "[ -s $result ]"; do
            if (( $(date +%s) >= deadline )); then
                state_set run_collect timeout '{"detail":"result timeout","vm":'"$n"'}'
                manifest_record_phase run_collect timeout $(( $(date +%s) - t0 ))
                die "VM N=$n did not produce $result_filename within ${timeout_s}s"
            fi
            sleep 2
        done
        log_to "  vm-$n result ready: $result"
    done

    # rsync each instance's output back
    log_to "collecting results from each instance's /out"
    mkdir -p "$RUN_DIR/work/instances"
    for n in "${instances[@]}"; do
        rsync -aq "$TARGET:$out_base/instance-$n/output/" "$RUN_DIR/work/instances/instance-$n/" \
            || die "rsync results for VM N=$n failed"
        cp "$RUN_DIR/work/instances/instance-$n/$result_filename" \
           "$RUN_DIR/results/$(basename "$result_filename" .json)_vm-$n.json"
    done

    # Validate the first instance's result against success criteria
    local result_path="$RUN_DIR/work/instances/instance-0/$result_filename"
    if [[ ! -s "$result_path" ]]; then
        die "expected result not found: $result_path"
    fi
    if ! jq -e . "$result_path" >/dev/null 2>&1; then
        die "result is not valid JSON: $result_path"
    fi
    local bench
    bench="$(jq -r '.benchmark // "?"' "$result_path")"
    local expected_bench
    expected_bench="$(desc_get '.success_criteria.benchmark_eq // "cpu_loop"')"
    if [[ "$bench" != "$expected_bench" ]]; then
        die "result .benchmark expected '$expected_bench', got '$bench'"
    fi

    # Artifacts
    remote "dmesg --ctime 2>/dev/null | tail -200" > "$RUN_DIR/artifacts/dmesg.tail.log" 2>/dev/null || true
    remote "journalctl -n 200 --no-pager 2>/dev/null || true" > "$RUN_DIR/artifacts/journal.tail.log" 2>/dev/null || true

    state_set run_collect success
    manifest_record_phase run_collect success $(( $(date +%s) - t0 ))
    log_to "run_collect OK ($result_path + ${#instances[@]} instance results)"
}

# --------------------------------------------------------------------------
# Cleanup trap (inverse-startup order; honors prior-state guard).

cleanup() {
    local exit_code=$?
    log_to "=== cleanup ==="

    # Only attempt the workload-side teardown if we actually started a run.
    if (( CLEANUP_STATE[run_started] == 1 )); then
        if (( ${CLEANUP_STATE[multi_vm_barrier_active]:-0} == 1 )); then
            # multi_vm_barrier teardown: kill local ssh launch processes
            # first (sends SIGHUP through to remote make+qemu), then
            # cover the rare straggler with a remote pkill, then flip
            # ossim back.
            log_to "tearing down multi_vm_barrier instances"
            local prefix="/home/yiliangw/ossim.local/prefix"
            for ssh_pid in ${CLEANUP_STATE[ssh_launch_pids]:-}; do
                kill -TERM "$ssh_pid" 2>/dev/null || true
            done
            if remote "pkill -TERM -f 'ossim-microbench-' 2>/dev/null; sleep 2; pkill -KILL -f 'ossim-microbench-' 2>/dev/null; true"; then
                manifest_record_cleanup "stop_qemu" success "pkill ossim-microbench-*"
            else
                manifest_record_cleanup "stop_qemu" failed "pkill error"
            fi
            manifest_record_cleanup "stop_ossimd" skipped "ossimd not used in microbench flow"
            if remote "$prefix/bin/ossimctl disable >/dev/null 2>&1 || true"; then
                manifest_record_cleanup "ossimctl_disable" success ""
            else
                manifest_record_cleanup "ossimctl_disable" failed ""
            fi
            manifest_record_cleanup "unload_ossim_module" skipped \
                "module-prior=${CLEANUP_STATE[ossim_module_loaded_prior]}; not unloaded by this run"
            manifest_record_cleanup "stop_vng" skipped "no vng instance started by this run"
        else
            # native (physical) phase: nothing started, but record skips
            # explicitly so the manifest cleanup section is complete.
            manifest_record_cleanup "stop_qemu" skipped "no qemu launched for native phase"
            manifest_record_cleanup "stop_ossimd" skipped "no ossimd launched for native phase"
            manifest_record_cleanup "ossimctl_disable" skipped "native phase does not touch ossim"
            manifest_record_cleanup "unload_ossim_module" skipped \
                "module-prior=${CLEANUP_STATE[ossim_module_loaded_prior]}; not loaded by this run"
            manifest_record_cleanup "stop_vng" skipped "no vng instance started by this run"
        fi
    fi

    release_remote_lock

    # Final manifest
    local final_phase final_class
    final_phase="$(jq -r '.current_phase // "preflight"' "$STATE_JSON" 2>/dev/null)"
    final_class="$(jq -r '.current_classification // "phase_fail"' "$STATE_JSON" 2>/dev/null)"
    if (( exit_code == 0 )) && [[ "$final_class" != "success" ]]; then
        final_class="success"
    elif (( exit_code != 0 )) && [[ "$final_class" != "phase_fail" \
                                  && "$final_class" != "timeout" \
                                  && "$final_class" != "cancelled" ]]; then
        # Any non-zero exit with a non-terminal classification (success
        # or in_progress) gets stamped as phase_fail — previously
        # in_progress slipped through and confused downstream tools.
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
