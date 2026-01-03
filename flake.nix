{
  description = "C/C++ development environment";
  inputs.nixpkgs.url = "https://flakehub.com/f/NixOS/nixpkgs/0"; # stable Nixpkgs

  outputs = {self, ...} @ inputs: let
    supportedSystems = [
      "x86_64-linux"
      "aarch64-linux"
      "x86_64-darwin"
      "aarch64-darwin"
    ];

    forEachSupportedSystem = f:
      inputs.nixpkgs.lib.genAttrs supportedSystems (
        system:
          f {
            pkgs = import inputs.nixpkgs {inherit system;};
          }
      );
  in {
    devShells = forEachSupportedSystem (
      {pkgs}: {
        default =
          pkgs.mkShell.override {}
          {
            buildInputs = with pkgs; [
              python312
              python312Packages.websockets
            ];

            packages = with pkgs;
              [
                ninja
                cmake
                asio
                boost
                gtest
                spdlog
                pkg-config
                clang-tools
                websocketpp
                pyright
              ]
              ++ (
                if system == "aarch64-darwin"
                then []
                else [gdb]
              );
          };
      }
    );
  };
}
