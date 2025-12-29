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
          export LOCALE_ARCHIVE="${pkgs.glibcLocales}/lib/locale/locale-archive"
          export LANG=en_US.UTF-8

          echo "-I${pkgs.glibc.dev}/include" > compile_flags.txt
          pkg-config --cflags ncursesw | tr ' ' '\n' >> compile_flags.txt
          echo $NIX_CFLAGS_COMPILE | tr ' ' '\n' >> compile_flags.txt
          fishConfig="
            function fish_prompt; set_color green; echo -n \"[laed]\"; set_color normal; echo \" :: \"; end;
          "
          exec ${pkgs.fish}/bin/fish -C "$fishConfig"
        '';
      };
    };
}
