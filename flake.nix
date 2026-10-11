{
  description = "Herramientas para investigar tarjetas JCOP3 con OpenSC";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { nixpkgs, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              bash
              git
              gnumake
              opensc
              pcsc-tools
            ];
            nativeBuildInputs = with pkgs; [
              autoconf
              automake
              libtool
              pkg-config
            ];
            buildInputs = with pkgs; [
              openssl
              pcsclite
              zlib
            ];
          };
        });
    };
}
