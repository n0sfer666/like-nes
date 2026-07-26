(module
  (memory (export "mem") 1)

  (func $sat (param $v i64) (result i32)
    (if (result i32) (i64.gt_s (local.get $v) (i64.const 2147483647))
      (then (i32.const 2147483647))
      (else
        (if (result i32) (i64.lt_s (local.get $v) (i64.const -2147483648))
          (then (i32.const -2147483648))
          (else (i32.wrap_i64 (local.get $v)))))))

  (func (export "gravity") (param $ptr i32) (param $n i32) (param $g i32) (param $dt i32)
    (local $i i32) (local $delta i32) (local $addr i32) (local $cur i32)
    (local.set $delta
      (call $sat (i64.shr_s
        (i64.mul (i64.extend_i32_s (local.get $g)) (i64.extend_i32_s (local.get $dt)))
        (i64.const 16))))
    (local.set $i (i32.const 0))
    (block $done
      (loop $loop
        (br_if $done (i32.ge_s (local.get $i) (local.get $n)))
        (local.set $addr (i32.add (local.get $ptr) (i32.mul (local.get $i) (i32.const 4))))
        (local.set $cur (i32.load (local.get $addr)))
        (i32.store (local.get $addr)
          (call $sat (i64.add (i64.extend_i32_s (local.get $cur)) (i64.extend_i32_s (local.get $delta)))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $loop))))
)
