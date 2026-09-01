# MetaSerious Backend: LAMP Deployment Guide

> **Scope**: this document describes how to install and run the existing MetaSerious
> middleware (PHP/Slim + MariaDB) on a fresh LAMP server. It does **not** cover the
> Unreal Engine client. See the main project README for that.

## 1. Stack overview

1. OS: Ubuntu 24.04 LTS
2. Apache: 2.4.x with `mod_rewrite`
3. PHP: 8.3 (CLI + `libapache2-mod-php` or PHP-FPM)
4. Database: MariaDB 10.11
5. Dependency manager: Composer 2.x

Application stack: [Slim Framework 4](https://www.slimframework.com/) (routing),
`vlucas/phpdotenv` (config), `guzzlehttp/guzzle` (outbound HTTP), `monolog/monolog`
(logging), `nyholm/psr7` (PSR-7 implementation).

## 2. Prerequisites

```bash
sudo apt update
sudo apt install apache2 mariadb-server php8.3 php8.3-cli php8.3-mysql \
    php8.3-mbstring php8.3-curl php8.3-xml php8.3-zip unzip

# Composer (if not already installed)
curl -sS https://getcomposer.org/installer | php
sudo mv composer.phar /usr/local/bin/composer

sudo a2enmod rewrite
sudo systemctl restart apache2
```

## 3. Get the code onto the server

The application expects to live at **`/var/www/metaserious`**. This path is
hard coded in a couple of places (`public/frontend/register.php`,
`public/index.php` log paths), so either deploy at that exact path or search/replace
if you need a different one.

```bash
sudo mkdir -p /var/www/metaserious
sudo chown -R $USER:$USER /var/www/metaserious
# copy the contents of this repo's backend/ folder into /var/www/metaserious
# (backend/public/index.php must end up at /var/www/metaserious/public/index.php)
```

Install PHP dependencies:

```bash
cd /var/www/metaserious
composer install --no-dev --optimize-autoloader
```

## 4. Apache virtual host

Document root must point at `public/`, not the project root, and requests that
don't match a real file must fall through to `index.php` (Slim front controller).

```apacheconf
<VirtualHost *:80>
    ServerName your-domain.example
    DocumentRoot /var/www/metaserious/public

    <Directory /var/www/metaserious/public>
        AllowOverride All
        Require all granted

        RewriteEngine On
        RewriteCond %{REQUEST_FILENAME} !-f
        RewriteCond %{REQUEST_FILENAME} !-d
        RewriteRule ^ index.php [QSA,L]
    </Directory>

    ErrorLog ${APACHE_LOG_DIR}/metaserious-error.log
    CustomLog ${APACHE_LOG_DIR}/metaserious-access.log combined
</VirtualHost>
```

```bash
sudo cp metaserious.conf /etc/apache2/sites-available/
sudo a2ensite metaserious.conf
sudo systemctl reload apache2
```

Use HTTPS (Let's Encrypt / `certbot`) in production. Session cookies and the
HMAC signed asset URLs assume a trusted transport.

## 5. Database

```bash
sudo mysql -u root -p <<'SQL'
CREATE DATABASE metaserious CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'metaserious'@'localhost' IDENTIFIED BY 'CHANGE_ME';
GRANT ALL PRIVILEGES ON metaserious.* TO 'metaserious'@'localhost';
FLUSH PRIVILEGES;
SQL

mysql -u metaserious -p metaserious < /path/to/db/metaserious-db.sql
```

The dump creates the `users`, `chats`, and `session_snapshots` tables. Application
users are created via `POST /api/v1/auth/register` (guarded by the `REGISTER_KID`
invite code below), not by seeding rows manually.

## 6. Configuration (`.env`)

`.env` lives at the project root (`/var/www/metaserious/.env`) and is **not**
committed to git; create it manually on each environment. Every key below is
mandatory; `config.php` throws at boot if any is missing or empty.

```ini
APP_ENV=prod
APP_DEBUG=false

API_BASE=https://your-domain.example/api/v1
CONTENT_ROOT=/var/www/metaserious/content/scenarios

# HMAC signed asset/session URLs
SIGN_TTL_SEC=60
ASSET_SIGN_SECRET=<generate with: openssl rand -hex 32>
REGISTER_KID=<invite code required by /api/v1/auth/register>

# Database
DB_HOST=localhost
DB_PORT=3306
DB_NAME=metaserious
DB_USER=metaserious
DB_PASS=<the password set in step 5>

# Logging
LOG_PATH=/var/www/metaserious/logs/app.log
LOG_LEVEL=info

# AI (text generation)
AI_PROVIDER=openai
AI_API_KEY=<your OpenAI API key>
AI_MODEL=gpt-4o-mini
AI_TIMEOUT=20
AI_TEMPERATURE=0.3
AI_MAX_TOKENS=400

# TTS
TTS_PROVIDER=elevenlabs
TTS_TIMEOUT=20
ELEVENLABS_API_KEY=<your ElevenLabs API key>
ELEVENLABS_VOICE_ID=<voice id>
ELEVENLABS_MODEL_ID=eleven_multilingual_v2
ELEVENLABS_STREAM_LATENCY=2
```

## 7. File permissions

```bash
mkdir -p /var/www/metaserious/logs
sudo chown -R www-data:www-data /var/www/metaserious/logs /var/www/metaserious/content
sudo chmod -R 750 /var/www/metaserious/logs
```

The app also writes to `/var/www/metaserious/logs/api-extern.txt` and
`api-intern.txt` (hard coded paths in `public/index.php`). Make sure that
directory is writable by the web server user.

## 8. Smoke test

```bash
curl https://your-domain.example/health
# {"ok":true,"ts":...,"php":"8.3.x","app_env":"prod"}

curl https://your-domain.example/list
# lists all registered routes
```

Then open `https://your-domain.example/frontend/login.php` in a browser and
register a test account with the `REGISTER_KID` invite code from `.env`.
