--TEST--
namespace detection 05
--EXTENSIONS--
vyrtue
--ENV--
PHP_VYRTUE_DEBUG_DUMP_NAMESPACE=1
PHP_VYRTUE_DEBUG_DUMP_USE=1
--SKIPIF--
<?php if (!VyrtueExt\DEBUG) die("skip: vyrtue not debug build"); ?>
--FILE--
<?php
use some\namespace\{ClassA, ClassB, ClassC as C};
use function some\namespace\{fn_a, fn_b, fn_c};
use const some\namespace\{ConstA, ConstB, ConstC};
--EXPECT--
VYRTUE_USE: some\namespace\ClassA => ClassA
VYRTUE_USE: some\namespace\ClassB => ClassB
VYRTUE_USE: some\namespace\ClassC => C
VYRTUE_USE: some\namespace\fn_a => fn_a
VYRTUE_USE: some\namespace\fn_b => fn_b
VYRTUE_USE: some\namespace\fn_c => fn_c
VYRTUE_USE: some\namespace\ConstA => ConstA
VYRTUE_USE: some\namespace\ConstB => ConstB
VYRTUE_USE: some\namespace\ConstC => ConstC
