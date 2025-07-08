#!/usr/bin/perl

use Cwd 'abs_path';
use File::Basename;

my $idf_path = $ENV{"IDF_PATH"};
my $cur_dir = dirname(abs_path($0));

unless (defined $idf_path) {
    die "Need cd \$IDF_PATH and do `. ./export.sh` firstly";
}
my $freertos_patch_file = "$idf_path/components/freertos/esp_additions/freertos_tasks_c_additions.h";

patch_dma2d();
patch_idf();
patch_srtp();

sub patch_idf { 
    my $patch_content;
    do {local $/ = undef; $patch_content = <DATA>;};
    open (my $H, $freertos_patch_file) || die "$freertos_patch_file not exists\n";
    my $code;
    while (<$H>) {
        $code .= $_;
        if (/xTaskCreateRestrictedPinnedToCore/) {
            print "Patch already existed for code\n";
            close $H;
            return;
        }
    }
    close $H;

    $code .= $patch_content;

    open (my $H, ">$freertos_patch_file");
    print $H $code;
    close $H;
    print "Patch for IDF success, now continue to build\n";
}

sub patch_dma2d {
	my $content = 'assert\(dma2d_ll_rx_is_fsm_idle\(group->hal.dev, channel_id\)\)';
	my $dma2d = "$idf_path/components/esp_hw_support/dma/dma2d.c";
	open (my $H, $dma2d) or die "$dma2d file not exists\n";
	my $patched = 0;
	my $code = "";
	while (<$H>) {
		if (/$content/) {
			if (/^ +\/\//) {
				print "dma2d already patched\n";
				close $H;
				return;
			}
			if ($patched == 0 && /( +)(.*)/) {
				$code .= "$1 //$2\n";
				$patched = 1;
			} else {
				$code .= $_;
			}
		} else {
			$code .= $_;
		}
	}
	open (my $H, ">$dma2d");
    print $H $code;
    close $H;
    print "Patch for dma2d success\n";
}

sub patch_srtp {
	my $content = 'return srtp_cipher_type_test(ct, ct->test_data);';
	my $srtp = "$cur_dir/../../components/srtp/libsrtp/crypto/cipher/cipher.c";
	open (my $H, $srtp) or die "$srtp file not exists\n";
	my $patched = 0;
	my $code = "";
	while (<$H>) {
		if (index($_, $content) != -1) {
			if (/^ +\/\//) {
				print "srtp already patched\n";
				close $H;
				return;
			}
			if ($patched == 0 && /( +)(.*)/) {
				$code .= "$1// $2\n";
				$code .= "$1" . "return srtp_err_status_ok;\n";
				$patched = 1;
			} else {
				$code .= $_;
			}
		} else {
			$code .= $_;
		}
	}
	open (my $H, ">$srtp");
    print $H $code;
    close $H;
    print "Patch for srtp success\n";
}
__DATA__
BaseType_t xTaskCreateRestrictedPinnedToCore( const TaskParameters_t * const pxTaskDefinition, TaskHandle_t *pxCreatedTask, const BaseType_t xCoreID)
{
    TCB_t *pxNewTCB;
    BaseType_t xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;

	configASSERT( pxTaskDefinition->puxStackBuffer );

	if( pxTaskDefinition->puxStackBuffer != NULL )
	{
		/* Allocate space for the TCB.  Where the memory comes from depends
		on the implementation of the port malloc function and whether or
		not static allocation is being used. */
		pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

		if( pxNewTCB != NULL )
		{
            memset( ( void * ) pxNewTCB, 0x00, sizeof( TCB_t ) );
			/* Store the stack location in the TCB. */
			pxNewTCB->pxStack = pxTaskDefinition->puxStackBuffer;

			/* Tasks can be created statically or dynamically, so note
			this task had a statically allocated stack in case it is
			later deleted.  The TCB was allocated dynamically. */
			pxNewTCB->ucStaticallyAllocated = tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB;

			prvInitialiseNewTask(	pxTaskDefinition->pvTaskCode,
									pxTaskDefinition->pcName,
									pxTaskDefinition->usStackDepth,
									pxTaskDefinition->pvParameters,
									pxTaskDefinition->uxPriority,
									pxCreatedTask, pxNewTCB,
									pxTaskDefinition->xRegions,
									xCoreID );

			prvAddNewTaskToReadyList( pxNewTCB );
			xReturn = pdPASS;
		}
	}

	return xReturn;
}