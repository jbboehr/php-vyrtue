# Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
{
  description = "php-vyrtue";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-24.05";
    nixpkgs-unstable.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils = {
      url = "github:numtide/flake-utils";
    };
    gitignore = {
      url = "github:hercules-ci/gitignore.nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    pre-commit-hooks = {
      url = "github:cachix/pre-commit-hooks.nix";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.nixpkgs-stable.follows = "nixpkgs";
      inputs.flake-utils.follows = "flake-utils";
      inputs.gitignore.follows = "gitignore";
    };
    ext-ast = {
      url = "github:nikic/php-ast";
      flake = false;
    };
  };

  outputs = {
    self,
    nixpkgs,
    nixpkgs-unstable,
    flake-utils,
    gitignore,
    pre-commit-hooks,
    ext-ast,
  } @ args:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
        pkgs-unstable = nixpkgs-unstable.legacyPackages.${system};
        lib = pkgs.lib;

        src' = gitignore.lib.gitignoreSource ./.;

        src = pkgs.lib.cleanSourceWith {
          name = "php-vyrtue-source";
          src = src';
          filter = gitignore.lib.gitignoreFilterWith {
            basePath = ./.;
            extraRules = ''
              .clang-format
              composer.json
              composer.lock
              .editorconfig
              .envrc
              .gitattributes
              .github
              .gitignore
              *.md
              *.nix
              flake.*
            '';
          };
        };

        makePhp = php:
          php.buildEnv {
            extraConfig = ''
              include_path = .:${ext-ast}/
            '';
            extensions = {
              enabled,
              all,
            }:
              enabled ++ [all.ast all.opcache];
          };

        makePackage = {
          php,
          debugSupport ? false,
        } @ args: let
          php = makePhp args.php;
        in
          pkgs.callPackage ./derivation.nix {
            inherit (php) buildPecl;
            inherit src php debugSupport;
          };

        makeCheck = package:
          package.overrideAttrs (old: {
            doCheck = true;
          });

        pre-commit-check = pre-commit-hooks.lib.${system}.run {
          src = src';
          hooks = {
            actionlint.enable = true;
            alejandra.enable = true;
            alejandra.excludes = ["\/vendor\/"];
            # I hate formatters
            #clang-format.enable = true;
            #clang-format.types_or = ["c" "c++"];
            #clang-format.files = "\\.(c|h)$";
            markdownlint.enable = true;
            markdownlint.excludes = ["LICENSE\.md"];
            shellcheck.enable = true;
          };
        };
      in rec {
        packages = rec {
          php81 = makePackage {php = pkgs.php81;};
          php82 = makePackage {php = pkgs.php82;};
          php83 = makePackage {php = pkgs.php83;};
          php84 = makePackage {php = pkgs-unstable.php84;};
          php81-debug = makePackage {
            php = pkgs.php81;
            debugSupport = true;
          };
          php82-debug = makePackage {
            php = pkgs.php82;
            debugSupport = true;
          };
          php83-debug = makePackage {
            php = pkgs.php83;
            debugSupport = true;
          };
          php84-debug = makePackage {
            php = pkgs-unstable.php84;
            debugSupport = true;
          };
          default = php81;
        };

        devShells = pkgs.lib.mapAttrs (k: package:
          pkgs.mkShell {
            inputsFrom = [package];
            buildInputs = with pkgs; [
              actionlint
              autoconf-archive
              clang-tools
              lcov
              gdb
              package.php
              package.php.packages.composer
              valgrind
            ];
            shellHook = ''
              ${pre-commit-check.shellHook}
              #ln -sf ${package.php.unwrapped.dev}/include/php/ .direnv/php-include
              export REPORT_EXIT_STATUS=1
              export NO_INTERACTION=1
              export PATH="$PWD/vendor/bin:$PATH"
              # opcache isn't getting loaded for tests because tests are run with '-n' and
              # nixos doesn't compile in opcache and relies on mkWrapper to load extensions
              export TEST_PHP_ARGS='-c ${package.php.phpIni}'
            '';
          })
        packages;

        checks = {
          inherit pre-commit-check;
          php81 = makeCheck packages.php81;
          php81-debug = makeCheck packages.php81-debug;
          php82 = makeCheck packages.php82;
          php82-debug = makeCheck packages.php82-debug;
          php83 = makeCheck packages.php83;
          php83-debug = makeCheck packages.php83-debug;
          php84 = makeCheck packages.php84;
          php84-debug = makeCheck packages.php84-debug;
        };

        formatter = pkgs.alejandra;
      }
    );
}
