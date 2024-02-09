--TEST--
attribute 01
--EXTENSIONS--
vyrtue
--SKIPIF--
<?php if (!VyrtueExt\DEBUG) die("skip: vyrtue not debug build"); ?>
--FILE--
<?php
namespace FooBar;
#[Attribute]
class MyAttribute1 {}
#[Attribute]
class MyAttribute2 {}
#[MyAttribute1, \VyrtueExt\Debug\SampleAttribute]
#[MyAttribute2]
class FooBar {
}
--EXPECT--
entering sample attribute
leaving sample attribute