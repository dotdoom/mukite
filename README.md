Mukite is a multi-threaded (pthread) lightweight XMPP MUC component written in POSIX-compliant ANSI C with emphasis on performance and stability.

## Requirements:
* GCC
* GNU Make (to use Makefile)
* pthreads (should be present on every POSIX system)

## Compiling:
gmake clean mukite

## Setup:
copy config.example to `your_vhost.config` and edit as needed (more comments inside the file).

## Running:
`./mukite your_vhost.config`  
or  
`./mukite <your_vhost.config`

## Handled signals:
* SIGUSR1: serialize all data (including current participants)
* SIGHUP: reload config file (only options marked with 'Restart required: NO' will take effect)
* SIGTERM, SIGQUIT: serialize all data an close the connection

## Important
Due to current implementation limitations, may only work correctly with XML attribute values enclosed in single quotes ( ' ). Works fine with ejabberd.