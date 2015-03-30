CREATE TABLE `dfs_log_table`(
`id` bigint auto_increment,
`url` varchar(256) NOT NULL DEFAULT '', 
`operator` varchar(32) NOT NULL DEFAULT '', 
`ip` varchar(32) NOT NULL, 
`pool` varchar(64) NOT NULL, 
`type` int(11) DEFAULT 0, 
`ts` DATETIME DEFAULT CURRENT_TIMESTAMP, 
PRIMARY KEY(`id`),
KEY(`url`)
) ENGINE=InnoDB DEFAULT CHARSET = latin1;

CREATE TABLE `dfs_lifecycle`(
`meta_key` varchar(256) NOT NULL DEFAULT '',
`meta_value` varchar(256) NOT NULL DEFAULT '',
PRIMARY KEY(`meta_key`)
) ENGINE=InnoDB DEFAULT CHARSET = latin1;

CREATE TABLE `dfs_user`(
`user` varchar(64) NOT NULL DEFAULT'',
`total_size` bigint DEFAULT 107374182400,
`used_size` bigint DEFAULT 0,
`size_day` bigint DEFAULT 10737418240,
`used_day` bigint DEFAULT 0,
`flag` TINYINT DEFAULT 0,
 PRIMARY KEY(`user`)
)ENGINE=InnoDB DEFAULT CHARSET=latin1;