#!/usr/bin/perl
use strict;

my ($log, $elf);
my $search_func;
my $search_addr;
my $filter_flag = 0;
my $last_malloc_addr;
my %last_malloc_info;

my %symbol;
my %tree;
my %address;

my $load_spiffs = 0;
my @spiffs_partition;
my $app_name;
my $addr2line_toolchain = "addr2line";

my @os_file = (
    "media_lib_*.c",
    "msg_q.c",
    "audio_mem.c",
    "linear_buffer.c",
    "heap.c",
    "codec_mem.c"
);

get_args();
guess_toolchain();
check_spiffs();
covert_os_file();
parse_file();

if ($last_malloc_addr) {
    print_malloc_info();
} else {
    gen_report();
}

sub help {
    print << 'USAGE';
please set your dump log and elf path: EX:
./mem_stat.pl malloc.log ./build/player_cli.elf
--flag malloc_flag (filter out by flag set by malloc)
--func filename:line (get all memory status from certain function address)
--last_malloc address (get last malloc stack which contain this address)
--load_spiffs (load spiffs files from flash)
USAGE
    exit(0);
}

sub short_path {
    my @path = split("/", $_[0]);
    my $p = 0;
    while ($p <= $#path) {
        if ($path[$p] eq '.') {
            splice(@path, $p, 1);
        }
        elsif ($path[$p] eq '..') {
            splice(@path, $p-1, 2);
            $p -= 1;
        } else {
            $p++;
        }
    }
    return join("/", @path);
}

sub get_args {
    my $i = 0;
    while ($i <= $#ARGV) {
        $_ = $ARGV[$i];
        if (/\.log/i) {
            $log = $_;
        } elsif (/\.elf/) {
            $elf = $_;
        } elsif (/--load_spiffs/) {
            $load_spiffs = 1;
        } elsif ($i < $#ARGV)  {
            $i++;
            if (/--flag/) {
                $filter_flag = $ARGV[$i];
            } elsif (/--func/) {
                $search_func = $ARGV[$i];
            } elsif (/--last_malloc/) {
                $last_malloc_addr = hex($ARGV[$i]);
            }
        }  
        $i++;
    }
    return if ($load_spiffs);
    unless (defined($log) && defined($elf)) {
        help();
    }
}

sub guess_toolchain {
    open(my $H, "build/CMakeCache.txt") || return;
    while (<$H>) {
        if (/CMAKE_ADDR2LINE:FILEPATH=(.*addr2line)/) {
            $addr2line_toolchain = $1;
            last;
        }
    }
    close $H;
}

sub covert_os_file {
    map{s/\./\\./} @os_file;
    map{s/\*/.*?/} @os_file;
    #print "os files: @os_file\n";
}

sub is_os_file {
    my $f = shift;
    for (@os_file) {
        if ($f =~ /$_/) {
            return 1;
        }
    }
    return 0;
}

sub add_symbol {
    my $addr = shift;
    return if (exists $symbol{$addr});
    my $func = `$addr2line_toolchain -e $elf $addr`;
    if ($func =~/\//) {
        $func = short_path($func);
    }
    if ($search_func && !$search_addr && $func =~/$search_func$/) {
        print "Find $addr mapped to func $func\n";
        $search_addr = $addr;                     
    }
    if (($func =~/esp-adf[-\w]+\/(.*?):(\d+)/ || 
        $func =~/esp-idf\/(.*?):(\d+)/ ||
        $func =~/esp-adf-libs-source\/(.*?):(\d+)/)) {
        $symbol{$addr} = ["root/$1", $2];
    }
    elsif (/(\w+.*):(\d+)/){
        $symbol{$addr} = ["root/$1", $2];
    } else {
        $symbol{$addr} = [];
    }
}

sub parse_all_pc {
    my @stack = @_;
    my $search_in_stack = 0;
    my $sel_symbol;
    my $contain = 0;
    for my $addr(@stack) {
        add_symbol($addr);
        #get parent call
        if ($search_func) {
            #find address firstly, return if not found
            next if (!$search_addr);
            if ($addr eq $search_addr) {
                $search_in_stack = 1;
                next;
            }
            next unless ($search_in_stack);
        }
        if ($#{$symbol{$addr}} > 0) {
            # not os file or last stack
            if ($addr eq $stack[$#stack] ||
                !is_os_file($symbol{$addr}->[0])) {
                $sel_symbol = $symbol{$addr};
                last;
            }
        }
    }
    unless ($sel_symbol) {
        die "not found symbol @stack\n";
        $sel_symbol = ["root/__undefined_funcs__", 0];
    }
    return $sel_symbol;
}

sub update_last_malloc_info {
    my ($addr, $size, $stack) = @_;
    if ($last_malloc_addr >= $addr &&
        $last_malloc_addr <= $addr + $size) {
        $last_malloc_info{-addr} = $addr;
        $last_malloc_info{-size} = $size;
        $last_malloc_info{-stack} = $stack;
        $last_malloc_info{-freed} = 0;
    }
}

sub remove_last_malloc {
    my $addr = shift;
    if ($addr == $last_malloc_info{-addr}) {
        $last_malloc_info{-freed} = 1;
    }
}

sub parse_file {
    my $act;
    my $buf;
    open (my $H, $log);
    binmode $H;
    seek $H, 0, 0;
    while (read($H, $act, 1)) {
        if ($act eq '+') {
            read($H, $buf, 11);
            my $h = $buf;
            my ($num, $r, $resv, $addr, $size) = unpack("CCCII", $buf);
            read($H, $buf, $num*4);
            my @stack = map {sprintf "%x", $_} unpack("I*", $buf);
            next if ($filter_flag && ($r & $filter_flag) == 0);
            if ($last_malloc_addr) {
                update_last_malloc_info($addr, $size, [@stack]);
                next; 
            }
            if ($addr == 0x3c1b1290) {
                print "@stack\n";
            }
            my $sel_symbol = parse_all_pc(@stack);
            my $leaf = get_leaf(@$sel_symbol);
            #malloc case
            #mem address size
            $address{$addr} = [$leaf, $size];# leaf, size 
            update_leaf($leaf, $size);
            #printf("malloc %x size %d num %d\n", $addr, $size, $num);
        }
        elsif ($act eq '-') {
            read($H, $buf, 5);
            my ($n, $addr) = unpack("CI", $buf);
            #printf("free %x\n", $addr);
            if ($last_malloc_addr) {
                remove_last_malloc($addr);
                next; 
            }
            if (exists $address{$addr}) {
                my $s = $address{$addr}->[1];
                my $p = $address{$addr}->[0];
                update_leaf($p, -$s);
                delete $address{$addr};
            }
        } else {
           my $pos = sprintf "%x", tell $H;
           print "File is truncated $act pos $pos\n";
           last;
        }
    }
    close $H;
    for (keys %address) {
        my $s = $address{$_}->[1];
        printf "Leak addr %x size $s\n", $_;
    }
}

sub print_malloc_info {
    if (exists $last_malloc_info{-addr}) {
        my $pos = $last_malloc_addr - $last_malloc_info{-addr};
        my $size = $last_malloc_info{-size};
        printf "Get last malloc buffer: %x position:$pos/$size freed:$last_malloc_info{-freed}\n",  
          $last_malloc_info{-addr};
        for my $addr(@{$last_malloc_info{-stack}}) {
            my $func = `addr2line -e $elf $addr`;
            if ($func =~/\//) {
                $func = short_path($func);
            }
            print "$func";
        }
    }
}

sub update_leaf {
    my ($leaf, $size) = @_;
    while ($leaf) {
        set_info($leaf, $size);
        $leaf = $leaf->{-parent};
    }
}

sub hex_it {
   my $s = shift;
   my $r;
   for (0..length($s)-1) {
      $r .= sprintf "%02x ", ord(substr($s, $_, 1));
   }
   $r;
}

sub get_leaf {
    my ($path, $line) = @_;
    my @p = split("/", $path);
    push @p, $line;

    if (!exists $tree{$p[0]}) {
        $tree{$p[0]} = {
            -name         => $p[0],
            -alloc_count  => 0,
            -free_count   => 0,
            -total_malloc => 0,
            -used         => 0,
            -max_use      => 0,
            -child        => {},
        };
    }
    my $parent = $tree{$p[0]};
    for (1..$#p) {
        my $c = $parent->{-child};
        if (!exists($c->{$p[$_]})) {
            $c->{$p[$_]} = {
                -name         => $p[$_],
                -alloc_count  => 0,
                -free_count   => 0,
                -total_malloc => 0,
                -used         => 0,
                -max_use      => 0,
                -child        => {},
                -parent       => $parent,
            };
        }
        $parent = $c->{$p[$_]};
    }
    return $parent;
}

sub set_info {
    my ($c, $size) = @_;
    if ($size > 0) {
        $c->{-alloc_count}++;
        $c->{-total_malloc} += $size;
    }
    else {
        $c->{-free_count}++;
    }
    #print "$c->{-name} + $size\n";
    $c->{-used} += $size;
    if ($c->{-used} > $c->{-max_use}) {
        $c->{-max_use} = $c->{-used};
    }
}

sub sort_by_use {
    return sort {$b->{-max_use} <=> $a->{-max_use}} @_;
}

sub print_leaf {
    my ($ident, $p) = @_;
    my $leak = $p->{-used} ? "  Leak: $p->{-used}" : "";
    print "$ident" . "├── ",  $p->{-name}, ":\tmax-use:", $p->{-max_use}, "$leak\n";
    $ident .= "│   ";
    for my $c(sort_by_use values %{$p->{-child}}) {
        print_leaf($ident, $c);
    }
}

sub gen_report {
    my $ident = "";
    for my $c (sort_by_use values(%tree)) {
        print_leaf($ident, $c);
    }
}

sub size {
    $_ = shift;
    if (/(\d+)K/) {
        return $1*1024;
    }
    if (/(\d+)M/) {
        return $1*1024*1024;
    }
    if (/0x/ || /[a-f]/i) {
        return hex($_);
    }
    return $_;
}

sub walker_file {
    my ($f, $func) = @_;
    open (my $H, $f) || return;
    while (<$H>) {
        $func->();
    }
    close $H;
}

sub get_app {
    walker_file("CMakeLists.txt",
        sub {
            if (/project\((\w+)\)/) {
                $app_name = $1;
                last;
            }
        });
    die "CMakeLists.txt wrong, run in app folder after build" unless($app_name);
}

sub spiffs_dump_pre_check {
    get_app();
    unless (-x 'mkspiffs') {
        print "mkspiffs not found, please install use:\n";
        print "git clone https://github.com/igrr/mkspiffs.git;";
        print "cd mkspiffs; ./build_all_configs.sh;\n";
        print "sudo cp mkspiffs /usr/bin; sudo chmod +x /usr/bin/mkspiffs;\n";
        die;
    }
    unless (-e "build/$app_name.elf") {
       die "Please rebuild for $app_name.elf not existed\n";
    }
}

sub get_partition_table {
    walker_file("idf.py partition-table|",
    sub {
        if (/\w+,data,spiffs,(\w+),(\w+),/) {
            @spiffs_partition = ($1, $2);
            @spiffs_partition = map {size($_)} @spiffs_partition;
            $spiffs_partition[1] = $spiffs_partition[1] / 4096 * 4096;
            @spiffs_partition = map {sprintf("0x%x", $_)} @spiffs_partition;
        }
    });
    die "Partition table not found" unless(@spiffs_partition);
    print "Got partition table @spiffs_partition\n";
}

sub dump_spiffs {
    if (-e "my_spiffs") {
        `rm spiffs.bin; rm -rf my_spiffs`;
    }
    print `esptool.py -b 460800 --before=default_reset read_flash  @spiffs_partition spiffs.bin`;
    print `mkspiffs -u my_spiffs spiffs.bin`;
    print "\n-------------------------------------------------------------------------\n";
}

sub get_log_file {
    my $file_name;
    walker_file("sdkconfig",
        sub {
            if (/CONFIG_MEDIA_LIB_MEM_TRACE_SAVE_PATH="\/\w+\/(.*?)"/) {
                $file_name = $1;
                last;
            }
        });
    die "CONFIG_MEDIA_LIB_MEM_TRACE_SAVE_PATH not set" unless ($file_name);
    return $file_name;
}

sub check_spiffs {
    return unless($load_spiffs);
    spiffs_dump_pre_check();
    get_partition_table();
    dump_spiffs();
    my $file_name = get_log_file();
    if (-e "my_spiffs/$file_name") {
        $log = "my_spiffs/$file_name";
    } else {
        $file_name = uc $file_name;
        $log = "my_spiffs/$file_name";
        die "history file not exists" unless(-e $log);
    }
    $log = "my_spiffs/$file_name";
    $elf = "build/$app_name.elf";
}
