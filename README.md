# graph-query-compiler
A LLVM-based query compiler written for the graph engine [Poseidon](https://github.com/dbis-ilm/poseidon_core)

Global definitions specify the interface that the DBMS must implement, primarily targeting graph-based processing.

### Supported Operators
- **Scan**
- **Traversal** (e.g., `ForeachRelationship`, `Expand`)
- **Basic Aggregations**
- **Joins**

Additionally, a simple interface supports an adaptive execution approach: a query initially runs through interpretation, and once compilation is complete, execution switches to the compiled version via a returned function pointer.

For integration, change global_defintions to desired DBMS.
