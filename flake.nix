{
  description = "ThinkBook NLCN3xWW Advanced BIOS menu unlock — patcher, analysis tooling, and Ghidra dev shell";

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
        packages = [
          pkgs.binutils
          pkgs.binwalk
          pkgs.python3Packages.capstone
          pkgs.cmake
          pkgs.file
          pkgs.gh
          pkgs.ghidra
          pkgs.git
          pkgs.gnumake
          pkgs.gnu-efi
          pkgs.innoextract
          pkgs.llvmPackages_21.lld
          pkgs.pkgsCross.mingwW64.stdenv.cc
          pkgs.p7zip
          pkgs.pkg-config
          pkgs.python3
          pkgs.radare2
          pkgs.ripgrep
          pkgs.uefitool
        ];
      };
    };
}
