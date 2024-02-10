--TEST--
replacement 07
--EXTENSIONS--
vyrtue
--SKIPIF--
<?php if (!VyrtueExt\DEBUG) die("skip: vyrtue not debug build"); ?>
--FILE--
<?php
namespace FooBar;
use VyrtueExt\Debug;
var_dump(Debug\sample_replacement_function());
--EXPECT--
entering sample function
int(12345)
