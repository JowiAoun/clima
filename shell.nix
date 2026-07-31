# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# `nix-shell` for people who do not have flakes turned on.
#
#   nix-shell
#
# It is the same shell `nix develop` gives you, from the same nixpkgs: the
# revision is read straight out of flake.lock rather than restated here, so the
# two cannot drift and `nix flake update` moves both at once.
#
# Note what this does *not* do: it does not fall back to <nixpkgs>. A channel
# would make this file work on a machine with no network and no lock discipline
# — and would hand that machine a different Qt from everyone else's, which is
# the exact failure the pin exists to prevent. If flake.lock cannot be honoured,
# better to fail loudly here.

let
  lock = builtins.fromJSON (builtins.readFile ./flake.lock);
  node = lock.nodes.nixpkgs.locked;

  # narHash is the hash of the unpacked tree, which is precisely what
  # fetchTarball checks — so the lockfile's own integrity field does the work
  # here with nothing restated.
  nixpkgs = builtins.fetchTarball {
    url = "https://github.com/${node.owner}/${node.repo}/archive/${node.rev}.tar.gz";
    sha256 = node.narHash;
  };
in

{ pkgs ? import nixpkgs { } }:

import ./nix/devshell.nix { inherit pkgs; }
