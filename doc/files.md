Used in 0.5.x
---------------------
* wallet.dat: personal wallet (BDB) with keys and transactions
* peers.dat: peer IP address database (custom format)
* blocks/blk000??.dat: block data (custom, 128 MiB per file); since 0.8.0
* blocks/rev000??.dat; block undo data (custom); since 0.8.0 (format changed since pre-0.8)
* blocks/index/*; block index (LevelDB); since 0.8.0
* chainstate/*; block chain state database (LevelDB); since 0.8.0
* database/*: BDB database environment; only used for wallet since 0.8.0
* novacoin.conf: configuration file
* debug.log: debugging messages of novacoin core
* db.log: debugging messages of database

Only used in pre-0.8.0
---------------------
* blktree/*; block chain index (LevelDB); since pre-0.8, replaced by blocks/index/* in 0.8.0
* coins/*; unspent transaction output database (LevelDB); since pre-0.8, replaced by chainstate/* in 0.8.0

Only used before 0.8.0
---------------------
* blkindex.dat: block chain index database (BDB); replaced by {chainstate/*,blocks/index/*,blocks/rev000??.dat} in 0.8.0
* blk000?.dat: block data (custom, 2 GiB per file); replaced by blocks/blk000??.dat in 0.8.0

