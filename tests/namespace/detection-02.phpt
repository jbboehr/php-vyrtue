--TEST--
namespace detection 02
--EXTENSIONS--
vyrtue
--ENV--
PHP_VYRTUE_DEBUG_DUMP_NAMESPACE=1
PHP_VYRTUE_DEBUG_DUMP_USE=1
--SKIPIF--
<?php if (!VyrtueExt\DEBUG) die("skip: vyrtue not debug build"); ?>
--FILE--
<?php
// from https://www.php.net/manual/en/language.namespaces.importing.php
namespace foo;
use My\Full\Classname as Another;

// this is the same as use My\Full\NSname as NSname
use My\Full\NSname;

// importing a global class
use ArrayObject;

// importing a function
use function My\Full\functionName;

// aliasing a function
use function My\Full\functionName as func;

// importing a constant
use const My\Full\CONSTANT;
--EXPECT--
VYRTUE_NAMESPACE: ENTER: foo
VYRTUE_USE: My\Full\Classname => Another
VYRTUE_USE: My\Full\NSname => NSname
VYRTUE_USE: ArrayObject => ArrayObject
VYRTUE_USE: My\Full\functionName => functionName
VYRTUE_USE: My\Full\functionName => func
VYRTUE_USE: My\Full\CONSTANT => CONSTANT
VYRTUE_NAMESPACE: LEFT: foo
