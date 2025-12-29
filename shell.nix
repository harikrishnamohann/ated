{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    clang
    clang-tools
    lldb
    ncurses
    pkg-config
  ];

  shellHook = ''
    export LOCALE_ARCHIVE="${pkgs.glibcLocales}/lib/locale/locale-archive"
    export LANG=en_US.UTF-8

    echo "-I${pkgs.glibc.dev}/include" > compile_flags.txt
    pkg-config --cflags ncursesw | tr ' ' '\n' >> compile_flags.txt
    echo $NIX_CFLAGS_COMPILE | tr ' ' '\n' >> compile_flags.txt
    echo "entered dev-shell: $(clang --version | head -n 1)"
  '';
}
