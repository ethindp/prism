# SPDX-License-Identifier: MPL-2.0
{
  description = "The the Platform-agnostic Reader Interface for Speech and Messages";
  inputs = {
    nixpkgs.url = "https://channels.nixos.org/nixos-26.05/nixexprs.tar.xz";
    nixpkgs-compat.url = "https://channels.nixos.org/nixos-22.05/nixexprs.tar.xz";
    godot-cpp = {
      url = "git+https://github.com/godotengine/godot-cpp?ref=4.5";
      flake = false;
    };
  };
  outputs =
    {
      self,
      nixpkgs,
      nixpkgs-compat,
      godot-cpp,
    }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems =
        f:
        nixpkgs.lib.genAttrs systems (
          system:
          f {
            inherit system;
            pkgs = nixpkgs.legacyPackages.${system};
            compat = nixpkgs-compat.legacyPackages.${system};
          }
        );
      inherit (nixpkgs) lib;
      llvmFor = pkgs: pkgs.llvmPackages_22;
      speechProviderFor =
        pkgs:
        pkgs.stdenv.mkDerivation {
          pname = "libspeechprovider";
          version = "1.0.3";
          src = pkgs.fetchFromGitHub {
            owner = "project-spiel";
            repo = "libspeechprovider";
            rev = "07e6a2a4f1df7149966588e57daf43336eba28ed";
            hash = "sha256-OUIK2PWnAh6zEqYV8a/YJw8L0bsewA5wnD3rEbIFZ3U=";
          };
          nativeBuildInputs = with pkgs; [
            meson
            ninja
            pkg-config
            glib
            gobject-introspection
            python3
          ];
          buildInputs = with pkgs; [ glib ];
          mesonFlags = [
            "-Dtests=false"
            "-Ddocs=false"
            "-Drust-bindings=false"
          ];
          meta.description = "Speech provider D-Bus interface library for Spiel";
        };
      spielFor =
        pkgs:
        pkgs.stdenv.mkDerivation {
          pname = "libspiel";
          version = "1.0.5-unstable-2026-01-09";
          src = pkgs.fetchFromGitHub {
            owner = "project-spiel";
            repo = "libspiel";
            rev = "adceb561f10cd0db335503b95273177b1a2b73b5";
            hash = "sha256-YiOz3irXgHeKcZmFuIiUmSVXTcH1HHHJ7plNcf8RMck=";
          };
          nativeBuildInputs = with pkgs; [
            meson
            ninja
            pkg-config
            glib
            gobject-introspection
            python3
          ];
          buildInputs = with pkgs; [
            glib
            gst_all_1.gstreamer
            gst_all_1.gst-plugins-base
            (speechProviderFor pkgs)
          ];
          mesonFlags = [
            "-Dtests=false"
            "-Ddocs=false"
            "-Dutils=false"
          ];
          meta.description = "Client library for the Spiel speech framework";
        };
      libsFor =
        pkgs: with pkgs; [
          speechd
          wayland
          wayland-scanner
          libxcb
          glib
          dbus
          gst_all_1.gstreamer
          gst_all_1.gst-plugins-base
          fmt
          simdutf
          (spielFor pkgs)
        ];
      toolsFor =
        pkgs: with pkgs; [
          cmake
          ninja
          pkg-config
          meson
          git
          zip
          wayland-scanner
          gobject-introspection
          gi-docgen
          (python3.withPackages (
            ps: with ps; [
              tappy
              python-dbusmock
            ]
          ))
        ];
      wineFor = pkgs: [ pkgs.wine64 ];
      cmakeWithoutSetupHook =
        pkgs:
        pkgs.runCommand "cmake-without-setup-hook" { } ''
          mkdir -p $out/bin
          ln -s ${pkgs.cmake}/bin/* $out/bin/
        '';
      releaseLibsFor =
        { pkgs, compat }:
        with compat;
        [
          speechd
          wayland
          xorg.libxcb
          glib
          dbus
          gst_all_1.gstreamer
          gst_all_1.gst-plugins-base
          (spielFor pkgs)
        ];
      releaseToolsFor =
        { pkgs, compat }:
        with compat;
        [
          (cmakeWithoutSetupHook pkgs)
          ninja
          pkg-config
          wayland
          git
          zip
          python3
        ];
      prismVersion =
        let
          lines = builtins.filter builtins.isString (
            builtins.split "\n" (builtins.readFile ./CMakeLists.txt)
          );
          hits = builtins.concatMap (
            l:
            let
              m = builtins.match "[[:space:]]*VERSION[[:space:]]+([0-9]+\\.[0-9]+\\.[0-9]+)[[:space:]]*" l;
            in
            if m == null then [ ] else m
          ) lines;
        in
        if hits == [ ] then
          throw "flake.nix: no 'VERSION x.y.z' line found in CMakeLists.txt"
        else
          builtins.head hits;
      mkShell =
        {
          pkgs,
          withWinelibs ? false,
          withLint ? false,
          withEmscripten ? false,
        }:
        pkgs.mkShell {
          name =
            "prism"
            + lib.optionalString withWinelibs "-winelibs"
            + lib.optionalString withEmscripten "-emscripten"
            + lib.optionalString withLint "-lint";
          PRISM_GODOT_CPP_SOURCE_DIR = "${godot-cpp}";
          buildInputs = libsFor pkgs;
          nativeBuildInputs =
            toolsFor pkgs
            ++ lib.optionals withWinelibs (wineFor pkgs)
            ++ lib.optionals withEmscripten [ pkgs.emscripten ]
            ++ lib.optionals withLint [
              (llvmFor pkgs).clang-tools
              (llvmFor pkgs).clang
            ];
          shellHook = lib.optionalString withEmscripten ''
            
                                    export EM_CACHE="''${EM_CACHE:-$PWD/.em_cache}"
                                    if [ ! -d "$EM_CACHE" ]; then
                                      mkdir -p "$EM_CACHE"
                                      cp -r --no-preserve=mode,ownership ${pkgs.emscripten}/share/emscripten/cache/. "$EM_CACHE" || true
                                    fi
          '';
        };
      mkReleaseShell =
        {
          pkgs,
          compat,
          withWinelibs ? false,
        }:
        (compat.mkShell.override { stdenv = compat.gcc12Stdenv; }) {
          name = "prism" + lib.optionalString withWinelibs "-winelibs";
          PRISM_GODOT_CPP_SOURCE_DIR = "${godot-cpp}";
          buildInputs = releaseLibsFor { inherit pkgs compat; };
          nativeBuildInputs =
            releaseToolsFor { inherit pkgs compat; } ++ lib.optionals withWinelibs (wineFor pkgs);
        };
      mkPrism =
        {
          pkgs,
          stdenv,
          libs,
          tools,
          shared ? true,
          buildType ? "Release",
          withWinelibs ? false,
          withGdextension ? false,
          withSpiel ? false,
          provider ? "BUNDLED",
        }:
        let
          cmakeFlags = [
            "-GNinja"
            "-DCMAKE_BUILD_TYPE=${buildType}"
            "-DBUILD_SHARED_LIBS=${if shared then "ON" else "OFF"}"
            "-DPRISM_ENABLE_TESTS=OFF"
            "-DPRISM_ENABLE_DEMOS=OFF"
            "-DPRISM_ENABLE_LEGACY_BACKENDS=ON"
            "-DPRISM_ENABLE_SHIMS=ON"
            "-DPRISM_ENABLE_GDEXTENSION=${if withGdextension then "ON" else "OFF"}"
            "-DPRISM_BUILD_WINELIBS=${if withWinelibs then "ON" else "OFF"}"
            "-DPRISM_ENABLE_SPIEL_BACKEND=${if withSpiel then "ON" else "OFF"}"
            "-DPRISM_FMT_PROVIDER=${provider}"
            "-DPRISM_SIMDUTF_PROVIDER=${provider}"
            "-DPRISM_CONCURRENTQUEUE_PROVIDER=BUNDLED"
          ]
          ++ lib.optionals withGdextension [ "-DPRISM_GODOT_CPP_SOURCE_DIR=${godot-cpp}" ];
        in
        stdenv.mkDerivation {
          pname = "prism";
          version = prismVersion;
          src = self;
          buildInputs = libs;
          nativeBuildInputs = tools ++ lib.optionals withWinelibs (wineFor pkgs);
          configurePhase = ''
            runHook preConfigure
            cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$out ${lib.escapeShellArgs cmakeFlags}
            runHook postConfigure
          '';
          buildPhase = ''
            runHook preBuild
            cmake --build build --parallel $NIX_BUILD_CORES
            runHook postBuild
          '';
          installPhase = ''
            runHook preInstall
            cmake --install build
            runHook postInstall
          '';
          meta = {
            description = "Cross-platform screen reader and TTS abstraction library";
            license = lib.licenses.mpl20;
            platforms = systems;
          };
        };
      mkReleasePrism =
        { pkgs, compat, ... }@args:
        mkPrism (
          builtins.removeAttrs args [ "compat" ]
          // {
            stdenv = compat.gcc12Stdenv;
            libs = releaseLibsFor { inherit pkgs compat; };
            tools = releaseToolsFor { inherit pkgs compat; };
          }
        );
    in
    {
      devShells = forAllSystems (
        { pkgs, compat, ... }: {
          default = mkReleaseShell { inherit pkgs compat; };
          winelibs = mkReleaseShell {
            inherit pkgs compat;
            withWinelibs = true;
          };
          lint = mkShell {
            inherit pkgs;
            withLint = true;
          };
          winelibs-lint = mkShell {
            inherit pkgs;
            withWinelibs = true;
            withLint = true;
          };
          gdextension-lint = mkShell {
            inherit pkgs;
            withLint = true;
          };
          emscripten = mkShell {
            inherit pkgs;
            withEmscripten = true;
          };
          emscripten-lint = mkShell {
            inherit pkgs;
            withEmscripten = true;
            withLint = true;
          };
        }
      );
      packages = forAllSystems (
        { pkgs, compat, ... }: {
          default = mkReleasePrism { inherit pkgs compat; };
          prism-static = mkReleasePrism {
            inherit pkgs compat;
            shared = false;
          };
          prism-debug = mkReleasePrism {
            inherit pkgs compat;
            buildType = "Debug";
          };
          prism-system = mkPrism {
            inherit pkgs;
            inherit (pkgs) stdenv;
            libs = libsFor pkgs;
            tools = toolsFor pkgs;
            provider = "SYSTEM";
          };
          gdextension = mkReleasePrism {
            inherit pkgs compat;
            withGdextension = true;
          };
          libspiel = spielFor pkgs;
          libspeechprovider = speechProviderFor pkgs;
        }
      );
      checks = forAllSystems (
        { system, ... }:
        self.packages.${system}
        // {
          shell-default = self.devShells.${system}.default;
          shell-winelibs = self.devShells.${system}.winelibs;
          shell-lint = self.devShells.${system}.lint;
          shell-winelibs-lint = self.devShells.${system}.winelibs-lint;
          shell-gdextension-lint = self.devShells.${system}.gdextension-lint;
          shell-emscripten = self.devShells.${system}.emscripten;
          shell-emscripten-lint = self.devShells.${system}.emscripten-lint;
        }
      );
    };
}
