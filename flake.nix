{
  description = "clementine text editor";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          pkg-config
          ncurses
        ];

        shellHook = ''
          export LOCALE_ARCHIVE="${pkgs.glibcLocales}/lib/locale/locale-archive"
          export LANG=en_US.UTF-8

          echo "-I${pkgs.glibc.dev}/include" > compile_flags.txt
          pkg-config --cflags ncurses | tr ' ' '\n' >> compile_flags.txt
          echo $NIX_CFLAGS_COMPILE | tr ' ' '\n' >> compile_flags.txt
         
          echo "entered dev-shell: $(clang --version | head -n 1)"
        '';
      };
    };
}
