{
  description = "libjcop3p11: soporte libre para firma digital con IAS/ECC y OpenSC";

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
            name = "libjcop3p11";

            shellHook = ''
              if [[ $- == *i* ]]; then
                PS1="(libjcop3p11) $PS1"
              fi
            '';

            packages = with pkgs; [
              bash
              git
              gnumake
              opensc
              pcsc-tools
              python3 # pcsc-spy decoder from pcsclite.dev
              util-linux # script: terminal fixture for authenticated launcher tests
              patchelf # relative paths in release libraries
              xz # export the Nix runtime closure
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
