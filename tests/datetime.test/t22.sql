DROP TABLE IF EXISTS t22
CREATE TABLE t22 (c INTEGER null)$$
INSERT INTO t22 VALUES (NULL)
ALTER TABLE t22 ALTER COLUMN c SET DATA TYPE DATETIME $$
SELECT type FROM comdb2_columns WHERE tablename="t22" AND columnname="c"
SELECT c FROM t22