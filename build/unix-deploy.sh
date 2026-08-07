#!/bin/bash

set -e

# ---------------------------------------------------------------------------
# Builds the uise-desktop demo applications in deployable form and packs
# them into a single distributable archive: a signed .dmg on macOS, a .zip
# on Linux. Companion to build/windows-deploy.bat.
#
# Usage (from the workspace root, one level above uise-desktop/):
#   uise_build=release ./uise-desktop/build/macos-deploy
#   uise_build=release ./uise-desktop/build/linux-deploy
#
# Env vars (all optional; matches build/unix-ci.sh where names overlap):
#   uise_compiler          clang | gcc                          (default: clang)
#   uise_build             release | debug | minsize_release    (default: release)
#   build_workers          parallel build jobs                  (default: 4)
#   QT_HOME                Qt install root (must contain bin/macdeployqt on macOS)
#   Boost_DIR               Boost cmake config dir
#   deps_universal_root     root of prebuilt deps (Boost etc), same default as unix-ci.sh
#   uise_build_dir          cmake build dir       (default: $PWD/builds/deploy-$uise_compiler-$uise_build)
#   uise_deploy_dir         output dir for the packaged archive (default: $PWD/deploy)
#   uise_deploy_version     version string used in output file names (default: parsed from CMakeLists.txt)
#   uise_deploy_skip_build  "yes" -> reuse an existing $uise_build_dir instead of configuring+building
# ---------------------------------------------------------------------------

if [ -z "$uise_compiler" ]; then
    export uise_compiler=clang
fi

if [ -z "$uise_build" ]; then
    export uise_build=release
fi

if [ -z "$build_workers" ]; then
    export build_workers=4
fi

export CC=clang
export CXX=clang++
if [[ "$uise_compiler" == "gcc" ]]; then
    export CC=gcc
    export CXX=g++
fi

self_path="`dirname \"$0\"`"
export self_path="`( cd \"$self_path\" && pwd )`"
export src_dir="$self_path/.."

if [ -z "$deps_universal_root" ]; then
    export deps_universal_root=$self_path/../../../../hatn/deps
fi

if [ -z "$Boost_DIR" ]; then
    export Boost_DIR=$deps_universal_root/root-$uise_compiler
fi

if [[ "$uise_build" == "release" ]]; then
    export build_type=Release
fi
if [[ "$uise_build" == "debug" ]]; then
    export build_type=Debug
fi
if [[ "$uise_build" == "minsize_release" ]]; then
    export build_type=MinSizeRel
fi

if [ -z "$uise_build_dir" ]; then
    export uise_build_dir="$PWD/builds/deploy-$uise_compiler-$uise_build"
fi

if [ -z "$uise_deploy_dir" ]; then
    export uise_deploy_dir="$PWD/deploy"
fi

if [ -z "$uise_deploy_version" ]; then
    uise_deploy_version="`grep -m1 -E 'PROJECT *\(uisedesktop VERSION' \"$src_dir/CMakeLists.txt\" | grep -o -E '[0-9]+\.[0-9]+\.[0-9]+'`"
    if [ -z "$uise_deploy_version" ]; then
        uise_deploy_version=0.0.0
    fi
    export uise_deploy_version
fi

uname_s="`uname -s`"
case "$uname_s" in
    Darwin) platform=macos ;;
    Linux)  platform=linux ;;
    *) echo "Unsupported platform for unix-deploy.sh: $uname_s" >&2; exit 1 ;;
esac
arch="`uname -m`"

echo "platform=$platform arch=$arch version=$uise_deploy_version"
echo "build_dir=$uise_build_dir"
echo "deploy_dir=$uise_deploy_dir"

mkdir -p "$uise_deploy_dir"

if [[ "$uise_deploy_skip_build" != "yes" ]]; then
    mkdir -p "$uise_build_dir"
    current_dir=$PWD
    cd "$uise_build_dir"
    cmake -DCMAKE_BUILD_TYPE=$build_type -DUISE_DESKTOP_DEMO=ON -DUISE_DESKTOP_TEST=OFF -DUISE_DESKTOP_DEMO_BUNDLE=ON "$src_dir"
    cmake --build . -j$build_workers
    cd "$current_dir"
fi

demo_bin_dir="$uise_build_dir/demo/bin"
if [ ! -d "$demo_bin_dir" ]; then
    echo "Demo output directory not found: $demo_bin_dir (build failed, or uise_deploy_skip_build=yes was set without a prior deploy build?)" >&2
    exit 1
fi

package_name="uise-demos-$uise_deploy_version-$platform-$arch"
stage_dir="$uise_build_dir/deploy-stage"
rm -rf "$stage_dir"
mkdir -p "$stage_dir"

if [[ "$platform" == "macos" ]]; then

    # ------------------------------------------------------------------
    # macOS: single .app bundle (manager + all demo binaries + one Qt
    # payload), macdeployqt, verify/repair non-Qt dylib rpaths, ad-hoc
    # sign inside-out, pack into a .dmg.
    # ------------------------------------------------------------------

    manager_bundle="$demo_bin_dir/uise-demo-manager.app"
    if [ ! -d "$manager_bundle" ]; then
        echo "uise-demo-manager.app not found at $manager_bundle -- was the build configured with -DUISE_DESKTOP_DEMO_BUNDLE=ON?" >&2
        exit 1
    fi

    dest_bundle="$stage_dir/UISE Demos.app"
    cp -R "$manager_bundle" "$dest_bundle"

    if ! command -v macdeployqt >/dev/null 2>&1; then
        if [ -n "$QT_HOME" ] && [ -x "$QT_HOME/bin/macdeployqt" ]; then
            export PATH="$QT_HOME/bin:$PATH"
        else
            echo "macdeployqt not found on PATH; set QT_HOME to your Qt install root." >&2
            exit 1
        fi
    fi

    # Every sibling demo executable is passed too, so macdeployqt patches
    # their rpaths and pulls in whatever extra Qt modules they need (e.g.
    # Multimedia for qrcodescanner-demo).
    executable_args=()
    for exe in "$dest_bundle/Contents/MacOS/"*; do
        base="`basename \"$exe\"`"
        if [ -f "$exe" ] && [ -x "$exe" ] && [[ "$base" != "uise-demo-manager" ]]; then
            executable_args+=("-executable=$exe")
        fi
    done

    macdeployqt "$dest_bundle" -verbose=1 "${executable_args[@]}"

    # ---- verify / repair remaining out-of-bundle dylib references ----
    # libuisedesktop.dylib / libZXing.*.dylib carry build-tree absolute
    # install names that macdeployqt does not rewrite (it only patches Qt's
    # own libraries and the frameworks it already knows about).
    frameworks_dir="$dest_bundle/Contents/Frameworks"
    mkdir -p "$frameworks_dir"

    fix_pass=1
    while [ "$fix_pass" -le 5 ]; do
        changed=0
        while IFS= read -r -d '' macho; do
            while IFS= read -r dep; do
                case "$dep" in
                    /usr/lib/*|/System/*|@rpath/*|@executable_path/*|@loader_path/*|"") continue ;;
                esac
                dep_base="`basename \"$dep\"`"
                if [ ! -f "$frameworks_dir/$dep_base" ]; then
                    if [ -f "$dep" ]; then
                        cp "$dep" "$frameworks_dir/$dep_base"
                        chmod u+w "$frameworks_dir/$dep_base"
                        install_name_tool -id "@rpath/$dep_base" "$frameworks_dir/$dep_base"
                        changed=1
                    else
                        echo "Cannot resolve dependency '$dep' referenced by '$macho'" >&2
                        exit 1
                    fi
                fi
                install_name_tool -change "$dep" "@rpath/$dep_base" "$macho"
            done < <(otool -L "$macho" 2>/dev/null | tail -n +2 | awk '{print $1}')
        done < <(find "$dest_bundle/Contents/MacOS" "$frameworks_dir" "$dest_bundle/Contents/PlugIns" -type f \( -perm -u+x -o -name '*.dylib' \) -print0 2>/dev/null)

        if [ "$changed" -eq 0 ]; then
            break
        fi
        fix_pass=$((fix_pass+1))
    done

    # ---- ad-hoc sign, inside-out: nested code first, bundle last ----
    # Qt ships some dependencies (QtCore, QtGui, ...) as .framework bundles
    # rather than loose .dylib files. A .framework must be signed as a whole
    # bundle (codesign then seals its Resources/Info.plist alongside the
    # Versions/A binary); signing the inner Versions/A/<Name> Mach-O file
    # directly -- which an executable-permission find match would otherwise
    # do -- leaves the framework's own signature invalid under `codesign
    # --verify --deep --strict`, so frameworks and loose dylibs are signed
    # via two separate passes below.
    find "$frameworks_dir" -type f -name '*.dylib' 2>/dev/null | while IFS= read -r item; do
        codesign --force --sign - --timestamp=none "$item"
    done
    find "$frameworks_dir" -maxdepth 1 -type d -name '*.framework' 2>/dev/null | while IFS= read -r item; do
        codesign --force --sign - --timestamp=none "$item"
    done
    find "$dest_bundle/Contents/PlugIns" -type f -perm -u+x 2>/dev/null | while IFS= read -r item; do
        codesign --force --sign - --timestamp=none "$item"
    done
    find "$dest_bundle/Contents/MacOS" -type f -perm -u+x | while IFS= read -r item; do
        codesign --force --sign - --timestamp=none "$item"
    done
    codesign --force --sign - --timestamp=none "$dest_bundle"
    codesign --verify --deep --strict "$dest_bundle"

    # ---- dmg ----
    ln -s /Applications "$stage_dir/Applications"
    dmg_path="$uise_deploy_dir/$package_name.dmg"
    rm -f "$dmg_path"
    hdiutil create -volname "UISE Demos $uise_deploy_version" -srcfolder "$stage_dir" -ov -format UDZO "$dmg_path"

    echo "Created $dmg_path"
    echo "Ad-hoc signed (unnotarized): first launch needs right-click -> Open, or 'xattr -dr com.apple.quarantine'."

elif [[ "$platform" == "linux" ]]; then

    # ------------------------------------------------------------------
    # Linux: no official Qt deploy tool, so collect the Qt/uise libraries
    # and plugins manually next to the demo binaries, then zip.
    # ------------------------------------------------------------------

    pkg_dir="$stage_dir/$package_name"
    mkdir -p "$pkg_dir/bin" "$pkg_dir/lib" "$pkg_dir/plugins"

    find "$demo_bin_dir" -maxdepth 1 -type f -perm -u+x -exec cp {} "$pkg_dir/bin/" \;

    qt_plugins_dir="`command -v qmake >/dev/null 2>&1 && qmake -query QT_INSTALL_PLUGINS 2>/dev/null`"
    if [ -z "$qt_plugins_dir" ] && [ -n "$QT_HOME" ]; then
        qt_plugins_dir="$QT_HOME/plugins"
    fi

    # Resolve every shared-library dependency of every packaged binary in one pass.
    all_libs="`ldd "$pkg_dir"/bin/* 2>/dev/null | awk '{print $3}' | sort -u`"
    for lib in $all_libs; do
        [ -f "$lib" ] || continue
        base="`basename \"$lib\"`"
        case "$base" in
            libQt6*.so*|libicu*.so*|libuisedesktop.so*|libZXing.so*)
                cp -L "$lib" "$pkg_dir/lib/$base"
                ;;
        esac
    done

    if [ -n "$qt_plugins_dir" ] && [ -d "$qt_plugins_dir" ]; then
        for sub in platforms xcbglintegrations wayland-shell-integration wayland-decoration-client wayland-graphics-integration-client imageformats iconengines platformthemes tls multimedia; do
            [ -d "$qt_plugins_dir/$sub" ] && cp -R "$qt_plugins_dir/$sub" "$pkg_dir/plugins/"
        done
    else
        echo "Warning: Qt plugins directory not found (qmake missing and QT_HOME unset); the package may not run on a machine without Qt installed." >&2
    fi

    cat > "$pkg_dir/bin/qt.conf" <<'EOF'
[Paths]
Prefix=..
Libraries=lib
Plugins=plugins
EOF

    cat > "$pkg_dir/run-uise-demos.sh" <<'SCRIPT_EOF'
#!/bin/bash
# Launches the UISE demo manager with the bundled Qt libraries/plugins on
# LD_LIBRARY_PATH / QT_PLUGIN_PATH. Demos launched from the manager inherit
# this environment, so this is the only entry point needed.
self_dir="`dirname \"$0\"`"
self_dir="`( cd \"$self_dir\" && pwd )`"
export LD_LIBRARY_PATH="$self_dir/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$self_dir/plugins:$QT_PLUGIN_PATH"
exec "$self_dir/bin/uise-demo-manager" "$@"
SCRIPT_EOF
    chmod +x "$pkg_dir/run-uise-demos.sh"

    zip_path="$uise_deploy_dir/$package_name.zip"
    rm -f "$zip_path"
    ( cd "$stage_dir" && zip -r -q "$zip_path" "$package_name" )

    echo "Created $zip_path"

fi
