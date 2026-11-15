{
  description = "unemu-at-bios: Sergey Kiselev's 8088_bios adapted as the open AT BIOS of the unemu machines";

  # Same nixpkgs revision as unemupc98, so both repositories share one toolchain
  # (the ROM bytes depend on the nasm version).
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/d407951447dcd00442e97087bf374aad70c04cea";

  outputs = { self, nixpkgs }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "x86_64-linux" "aarch64-linux" ];
      forAll = f: nixpkgs.lib.genAttrs systems (system: f system);
      pkgsFor = system: import nixpkgs { inherit system; };

      # One build variant: the raw 32 KiB image that maps at F000:8000 (physical
      # F8000h-FFFFFh), assembled from src/bios.asm with the given defines. The
      # 128 KiB flash-programmer images of the upstream Makefile are not built.
      # `patches` (none at present) are applied to a copy of the source before
      # assembly.
      romFor = system: { name, defines, patches ? [ ], sha256 ? null }:
        let pkgs = pkgsFor system; in
        pkgs.stdenv.mkDerivation {
          pname = "unemu-at-bios-${name}";
          version = "1.0.2-unemu";
          src = ./.;
          nativeBuildInputs = [ pkgs.nasm pkgs.gnupatch ];
          dontConfigure = true;
          buildPhase = ''
            runHook preBuild
            for p in ${builtins.concatStringsSep " " (map toString patches)}; do
              patch --batch --forward --fuzz=0 -p1 < "$p"
            done
            nasm -isrc/ ${builtins.concatStringsSep " " (map (d: "-D${d}") defines)} \
              -O9 -f bin -o bios.bin -l bios.lst src/bios.asm -w-all
            size=$(wc -c < bios.bin)
            if [ "$size" -ne 32768 ]; then
              echo "unexpected BIOS size: $size bytes (expected 32768)" >&2
              exit 1
            fi
            runHook postBuild
          '';
          installPhase = ''
            runHook preInstall
            mkdir -p $out
            cp bios.bin $out/bios-${name}.bin
            cp bios.lst $out/bios-${name}.lst
            sha256sum $out/bios-${name}.bin | cut -d' ' -f1 > $out/bios-${name}.bin.sha256
            ${if sha256 == null then "" else ''
              if [ "$(cat $out/bios-${name}.bin.sha256)" != "${sha256}" ]; then
                echo "SHA-256 of bios-${name}.bin is $(cat $out/bios-${name}.bin.sha256), pinned ${sha256}" >&2
                exit 1
              fi
            ''}
            runHook postInstall
          '';
        };

      variants = system: {
        # Upstream's Xi 8088 configuration, unchanged: the reference the unemu
        # changes are measured against. Pinned to the bytes unemupc98 already uses.
        xi8088 = romFor system {
          name = "xi8088";
          defines = [ "MACHINE_XI8088" ];
          sha256 = "1ad458acb1f22dee6b3d9b6bd95e83218a7ddc5389fa2b2c67a4001e689c81a8";
        };
        # The image unemupc98's open AT profile booted before unemu-at: Xi 8088
        # plus the diskette-reset fix (FLOPPY_RESET_ANY_DRIVE, formerly
        # patches/at-reset.patch). Same bytes as the former patched build.
        xi8088-atreset = romFor system {
          name = "xi8088-atreset";
          defines = [ "MACHINE_XI8088" "FLOPPY_RESET_ANY_DRIVE" ];
          sha256 = "61589573c9809273198b137d9419c87242b0a5f63c2ad834a9565950c3d6becc";
        };
        # The open AT BIOS of the unemu machines: the MACHINE_UNEMU_AT
        # configuration (see README.unemu.md for its differences from Xi 8088).
        unemu-at = romFor system {
          name = "unemu-at";
          defines = [ "MACHINE_UNEMU_AT" ];
          sha256 = "6938dc8b665fa120f33775163ab6bacc559389525db87adedf734416f9ef2442";
        };
      };
    in
    {
      packages = forAll (system: (variants system) // { default = (variants system).unemu-at; });

      # `nix flake check` rebuilds every pinned variant and fails on any byte change.
      checks = forAll (system: variants system);

      devShells = forAll (system:
        let pkgs = pkgsFor system; in {
          default = pkgs.mkShell {
            packages = [ pkgs.nasm pkgs.gnupatch pkgs.gnumake pkgs.cmake ];
          };
        });
    };
}
