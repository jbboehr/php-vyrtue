--TEST--
replacement 01
--EXTENSIONS--
vyrtue
--SKIPIF--
<?php if (!VyrtueExt\DEBUG) die("Skipped: vyrtue not debug build"); ?>
--FILE--
<?php
namespace FooBar;
use function VyrtueExt\Debug\sample_replacement_function;
var_dump(sample_replacement_function());
--EXPECT--
int(12345)
