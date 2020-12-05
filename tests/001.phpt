--TEST--
Check for intset presence
--SKIPIF--
<?php if (!extension_loaded("intset")) print "skip"; ?>
--FILE--
<?php 
echo "intset extension is available";
--EXPECT--
intset extension is available
