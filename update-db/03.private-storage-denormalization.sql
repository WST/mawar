
ALTER TABLE private_storage CHANGE id_user username VARCHAR(40) NOT NULL;
CREATE UNIQUE INDEX block ON private_storage(username, block_xmlns);
