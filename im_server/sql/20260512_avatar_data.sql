USE `im_server`;

ALTER TABLE `im_user`
    MODIFY COLUMN `avatar_url` MEDIUMTEXT COMMENT '头像URL或data URL';

ALTER TABLE `im_friend`
    MODIFY COLUMN `friend_avatar` MEDIUMTEXT COMMENT '好友头像';
