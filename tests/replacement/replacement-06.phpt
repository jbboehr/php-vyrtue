--TEST--
replacement 06
--EXTENSIONS--
vyrtue
--SKIPIF--
<?php if (!VyrtueExt\DEBUG) die("skip: vyrtue not debug build"); ?>
--FILE--
<?php
namespace FooBar;
var_dump(\VyrtueExt\Debug\sample_replacement_function());
--EXPECT--
entering sample function
int(12345)
