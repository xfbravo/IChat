-- 允许群头像和群成员头像保存 data URL。
-- 新部署的表结构已在 init_database.sql 中同步；该迁移用于已有数据库。

ALTER TABLE `im_group`
    MODIFY COLUMN `group_avatar` MEDIUMTEXT COMMENT '群头像URL或data URL';

ALTER TABLE `im_group_member`
    MODIFY COLUMN `avatar_url` MEDIUMTEXT COMMENT '头像URL或data URL';
