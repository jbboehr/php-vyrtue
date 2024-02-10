--TEST--
info
--EXTENSIONS--
vyrtue
--FILE--
<?php
phpinfo(INFO_MODULES);
--EXPECTF--
%A
vyrtue
%A
Version => %A
Released => %A
Authors => %A
%A
143 => vyrtue internal
545 => vyrtue internal
542 => vyrtue internal
516 => vyrtue internal
70 => vyrtue internal
%A
