# ecs-prototype
Entity-Component System prototype, unsual in that it is designed to work
on preallocated, fixed-size memory and does not use maps.
This could bring benefits in speed of random access to components of entities
as here it's just an index dispatch, rather than map search.
Of course this solution comes with some memory overhead so its meant
to be used only for specific cases, possibly with some other system in parallel.
Memory overhead can be calculated with this formula:

![overhead formula](./overhead.png)

No benchmarks yet!
