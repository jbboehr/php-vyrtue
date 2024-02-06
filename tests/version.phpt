--TEST--
version
--EXTENSIONS--
vyrtue
--FILE--
<?php
var_dump(VyrtueExt\VERSION);
--EXPECTF--
string(%d) "%d.%d.%d"