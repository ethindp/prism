# SPDX-License-Identifier: MPL-2.0
{
  description = "The the Platform-agnostic Reader Interface for Speech and Messages";
  inputs = {
    nixpkgs.url = "https://channels.nixos.org/nixos-26.05/nixexprs.tar.xz";
    godot-cpp = {
      url = "git+https://github.com/godotengine/godot-cpp?ref=4.5";
      flake = false;
    };
  };
  outputs =
    {
      self,
      nixpkgs,
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
          glib
          glibmm_2_68
          libsigcxx30
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
      mkPrism =
        {
          pkgs,
          shared ? true,
          buildType ? "Release",
          withWinelibs ? false,
          withGdextension ? false,
          withSpiel ? false,
          provider ? "BUNDLED",
        }:
        pkgs.stdenv.mkDerivation {
          pname = "prism";
          version = prismVersion;
          src = self;
          buildInputs = libsFor pkgs;
          nativeBuildInputs = toolsFor pkgs ++ lib.optionals withWinelibs (wineFor pkgs);

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
          meta = {
            description = "Cross-platform screen reader and TTS abstraction library";
            license = lib.licenses.mpl20;
            platforms = systems;
          };
        };
    in
    {
      devShells = forAllSystems (
        { pkgs, ... }: {
          default = mkShell { inherit pkgs; };
          winelibs = mkShell {
            inherit pkgs;
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
        { pkgs, ... }: {
          default = mkPrism { inherit pkgs; };
          prism-static = mkPrism {
            inherit pkgs;
            shared = false;
          };
          prism-debug = mkPrism {
            inherit pkgs;
            buildType = "Debug";
          };
          prism-system = mkPrism {
            inherit pkgs;
            provider = "SYSTEM";
          };
          gdextension = mkPrism {
            inherit pkgs;
            withGdextension = true;
          };
          libspiel = spielFor pkgs;
          libspeechprovider = speechProviderFor pkgs;
        }
      );
      checks = forAllSystems (
        { pkgs, system }:
        self.packages.${system}
        // {
          shell-default = self.devShells.${system}.default;
          shell-winelibs = self.devShells.${system}.winelibs;
          shell-lint = self.devShells.${system}.lint;
          shell-winelibs-lint = self.devShells.${system}.winelibs-lint;
          shell-emscripten = self.devShells.${system}.emscripten;
          shell-emscripten-lint = self.devShells.${system}.emscripten-lint;
        }
      );
    };
}
