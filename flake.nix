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
          fishConfig="
            function fish_prompt; set_color green; echo -n \"[ated]\"; set_color normal; echo \" :: \"; end;

            function build
              set mode \$argv[1]
              set SRC ./src/main.c
              set TARGET \"ated\"
              
              set CFLAGS \"-lncurses\" \"-std=gnu17\"

              if test \"\$mode\" = \"debug\"
                echo \"[Debug Build]\"
                clang \$SRC \$CFLAGS -Wall -fsanitize=address -g -o \$TARGET
              else if test \"\$mode\" = \"release\"
                echo \"[Release Build]\"
                clang \$SRC -O3 \$CFLAGS -o \$TARGET
              else
                echo \"Usage: build [debug|release]\"
              end
            end

            # Shortcuts
            abbr --add b \"build debug\"
            abbr --add br \"build release\"
            abbr --add run \"build debug && ./ated\"
          "
          exec ${pkgs.fish}/bin/fish -C "$fishConfig"
        '';
      };
    };
}
