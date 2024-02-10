--TEST--
replacement 08
--EXTENSIONS--
vyrtue
--SKIPIF--
<?php if (!VyrtueExt\DEBUG) die("skip: vyrtue not debug build"); ?>
--FILE--
<?php
namespace FooBar;
use VyrtueExt\Debug as VyrtueExtDebug;
var_dump(VyrtueExtDebug\sample_replacement_function());
--EXPECT--
entering sample function
int(12345)
