{
  description = "A terminal experiment";

  inputs = {
    nixpkgs.url = "nixpkgs/nixos-25.11";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          ncurses
          pkg-config
          clang
        ];

        shellHook = ''
          echo "-I${pkgs.glibc.dev}/include" > compile_flags.txt
          pkg-config --cflags ncurses | tr ' ' '\n' >> compile_flags.txt
          echo $NIX_CFLAGS_COMPILE | tr ' ' '\n' >> compile_flags.txt
          exec fish
        '';
      };
    };
}
