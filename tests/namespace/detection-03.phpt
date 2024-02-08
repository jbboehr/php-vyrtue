--TEST--
namespace detection 03
--EXTENSIONS--
vyrtue
--ENV--
PHP_VYRTUE_DEBUG_DUMP_NAMESPACE=1
PHP_VYRTUE_DEBUG_DUMP_USE=1
--SKIPIF--
<?php if (!VyrtueExt\DEBUG) die("skip: vyrtue not debug build"); ?>
--FILE--
<?php
namespace FooBar {
    use My\Full\Classname as Another;
    use My\Full\NSname;
    use function My\Full\functionName;
    use function My\Full\functionName as func;
}
namespace BarBat {
    use My\Full\Classname as Another;
    use My\Full\NSname;
    use function My\Full\functionName;
    use function My\Full\functionName as func;
}
--EXPECT--
VYRTUE_NAMESPACE: ENTER: FooBar
VYRTUE_USE: My\Full\Classname => Another
VYRTUE_USE: My\Full\NSname => NSname
VYRTUE_USE: My\Full\functionName => functionName
VYRTUE_USE: My\Full\functionName => func
VYRTUE_NAMESPACE: LEFT: FooBar
VYRTUE_NAMESPACE: ENTER: BarBat
VYRTUE_USE: My\Full\Classname => Another
VYRTUE_USE: My\Full\NSname => NSname
VYRTUE_USE: My\Full\functionName => functionName
VYRTUE_USE: My\Full\functionName => func
VYRTUE_NAMESPACE: LEFT: BarBat
