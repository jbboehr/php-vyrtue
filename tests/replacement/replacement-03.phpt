--TEST--
replacement 03
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
        return (new class {
            public function sayHello() {
                var_dump(sample_replacement_function());        
            }
        })->sayHello();
    }
}
(new BarBat())->sayHello();
--EXPECT--
entering sample function
int(12345)
