#!/usr/bin/env bash
set -euo pipefail

if [[ ${1:-} == --help || ${1:-} == -h ]]; then
    echo "Usage: $0 [ref [output-directory]]"
    echo "Defaults: HEAD, the repository's parent directory."
    exit 0
fi
if (( $# > 2 )); then
    echo "Usage: $0 [ref [output-directory]]" >&2
    exit 2
fi

source_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
release_ref=${1:-HEAD}
output_dir=${2:-"$source_root/.."}

# Check each initialized repository: ignore settings in a nested submodule
# can otherwise hide its changes from the top-level status command.
worktree_status=$(
    git -C "$source_root" status --porcelain=v1 \
        --untracked-files=all --ignore-submodules=none || exit
    git -C "$source_root" submodule foreach --quiet --recursive '
        changes=$(git status --porcelain=v1 --untracked-files=all \
            --ignore-submodules=none) || exit
        if test -n "$changes"; then
            printf "Submodule %s:\n%s\n" "$displaypath" "$changes"
        fi
    '
)
if [[ -n "$worktree_status" ]]; then
    echo "Error: release requires a clean worktree, including submodules." >&2
    printf '%s\n' "$worktree_status" >&2
    exit 1
fi

commit=$(git -C "$source_root" rev-parse --verify --end-of-options "$release_ref^{commit}")
name="ossim-$(git -C "$source_root" rev-parse --short "$commit")"

# Populate missing submodules in the current checkout without downloading full
# histories. The clean-worktree check above protects existing checkouts.
git -C "$source_root" submodule update --init --recursive --checkout --depth 1

# Once submodules are available, package exclusively from local repositories.
export GIT_ALLOW_PROTOCOL=file

stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT

# Resolve every pinned commit locally before cloning any repository. Read
# .gitmodules from the selected revision so older releases keep their pins.
collect_repositories() {
    local source_tree=$1 git_dir=$2 revision=$3 archive_path=$4
    local entry key module_name module_path module_commit module_git_dir

    # Cached repositories may refer to worktrees that no longer exist. Object
    # reads only need an existing working directory, not that original checkout.
    if ! git --git-dir="$git_dir" --work-tree="$source_root" \
        cat-file -e "$revision^{commit}" 2>/dev/null; then
        echo "Error: local repository for $archive_path lacks commit $revision." >&2
        echo "Populate the required submodule locally before packaging." >&2
        return 1
    fi
    printf '%s\0%s\0%s\0' "$git_dir" "$archive_path" "$revision"
    if ! git --git-dir="$git_dir" --work-tree="$source_root" \
        cat-file -e "$revision:.gitmodules" 2>/dev/null; then
        return 0
    fi

    {
        git --git-dir="$git_dir" --work-tree="$source_root" config --blob "$revision:.gitmodules" \
            --null --get-regexp '^submodule\..*\.path$' || [[ $? -eq 1 ]]
    } | while IFS= read -r -d '' entry; do
        key=${entry%%$'\n'*}
        module_name=${key#submodule.}
        module_name=${module_name%.path}
        module_path=${entry#*$'\n'}
        module_commit=$(git --git-dir="$git_dir" --work-tree="$source_root" \
            rev-parse --verify "$revision:$module_path")

        if [[ -e "$source_tree/$module_path/.git" ]]; then
            module_git_dir=$(git -C "$source_tree/$module_path" rev-parse --absolute-git-dir)
        else
            # Deinitialized submodules may still have a cached local repository.
            module_git_dir=$(git --git-dir="$git_dir" --work-tree="$source_root" rev-parse \
                --path-format=absolute --git-path "modules/$module_name")
        fi
        collect_repositories "$source_tree/$module_path" "$module_git_dir" \
            "$module_commit" "$archive_path/$module_path"
    done
}

collect_repositories "$source_root" \
    "$(git -C "$source_root" rev-parse --absolute-git-dir)" "$commit" "$name" \
    > "$stage/repositories"

mkdir -p -- "$output_dir"
output_dir=$(cd -- "$output_dir" && pwd)
while IFS= read -r -d '' git_dir &&
      IFS= read -r -d '' archive_path &&
      IFS= read -r -d '' revision; do
    destination="$stage/$archive_path"
    # The source is local; --no-local disables hardlink copying so Git honors
    # --depth. Both clone and any fetch below read only this local repository.
    git clone --no-local --depth 1 --no-tags --no-checkout -- "$git_dir" "$destination"
    if ! git -C "$destination" cat-file -e "$revision^{commit}" 2>/dev/null; then
        git -C "$destination" fetch --depth 1 --no-tags origin "$revision"
    fi
    git -C "$destination" checkout --detach "$revision"
done < "$stage/repositories"

# Exclude both the top-level .git directory and submodule .git files.
# Finish the archive before replacing an existing release with the same name.
tar --exclude='.git' -czf "$stage/$name.tar.gz" -C "$stage" "$name"
mv -- "$stage/$name.tar.gz" "$output_dir/$name.tar.gz"
sha256sum -- "$output_dir/$name.tar.gz"
