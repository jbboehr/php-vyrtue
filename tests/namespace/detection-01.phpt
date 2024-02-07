--TEST--
namespace detection 01
--EXTENSIONS--
vyrtue
--ENV--
PHP_VYRTUE_DEBUG_DUMP_NAMESPACE=1
PHP_VYRTUE_DEBUG_DUMP_USE=1
--SKIPIF--
<?php if (!VyrtueExt\DEBUG) die("Skipped: vyrtue not debug build"); ?>
--FILE--
<?php
namespace FooBar;
use function Foo\bar;
use function Foo\bat as booyah;
--EXPECT--
VYRTUE_NAMESPACE: ENTER: FooBar
VYRTUE_USE: Foo\bar => bar
VYRTUE_USE: Foo\bat => booyah
VYRTUE_NAMESPACE: LEFT: FooBar
