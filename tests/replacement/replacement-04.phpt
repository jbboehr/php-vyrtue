--TEST--
replacement 04
--EXTENSIONS--
vyrtue
--SKIPIF--
<?php if (!VyrtueExt\DEBUG) die("skip: vyrtue not debug build"); ?>
--FILE--
<?php
namespace FooBar;
use function VyrtueExt\Debug\sample_replacement_function;
class BarBat {
    public function sayHello() {
        return (function () {
            var_dump(sample_replacement_function());
        })();
    }
}
(new BarBat())->sayHello();
--EXPECT--
entering sample function
int(12345)
