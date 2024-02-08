--TEST--
namespace detection 04
--EXTENSIONS--
vyrtue
--ENV--
PHP_VYRTUE_DEBUG_DUMP_NAMESPACE=1
PHP_VYRTUE_DEBUG_DUMP_USE=1
--SKIPIF--
<?php if (!VyrtueExt\DEBUG) die("skip: vyrtue not debug build"); ?>
--FILE--
<?php
use My\Full\Classname as Another, My\Full\NSname;
--EXPECT--
VYRTUE_USE: My\Full\Classname => Another
VYRTUE_USE: My\Full\NSname => NSname
