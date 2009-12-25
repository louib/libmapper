
#include "../src/mapper_internal.h"
#include <mapper/mapper.h>
#include <stdio.h>
#include <math.h>

#include <unistd.h>
#include <arpa/inet.h>

mapper_admin my_admin = NULL;

int test_admin()
{
    int error=0, wait;

    my_admin = mapper_admin_new("tester", MAPPER_DEVICE_SYNTH, 8000);
    if (!my_admin)
    {
        printf("Error creating admin structure.\n");
        return 1;
    }
    printf("Admin structure initialized.\n");

    printf("Found interface %s has IP %s\n", my_admin->interface,
           inet_ntoa(my_admin->interface_ip));

    while (    !my_admin->port.locked
            || !my_admin->ordinal.locked )
    {
        usleep(10000);
        mapper_admin_poll(my_admin);
    }

    printf("Allocated port %d.\n", my_admin->port.value);
    printf("Allocated ordinal %d.\n", my_admin->ordinal.value);

    printf("Delaying for 5 seconds..\n");
    wait = 500;
    while (wait-- >= 0)
    {
        usleep(10000);
        mapper_admin_poll(my_admin);
    }

    mapper_admin_free(my_admin);
    printf("Admin structure freed.\n");

    return error;
}

int test_controller()
{
    mapper_device md = mdev_new("tester", 9000);
    if (!md) goto error;
    printf("Mapper device created.\n");

    mapper_signal sig =
        msig_float(1, "/testsig", 0, INFINITY, INFINITY, 0);

    mdev_register_output(md, sig);

    printf("Output signal /testsig registered.\n");

    printf("Number of outputs: %d\n", mdev_num_outputs(md));

    const char *host = "localhost";
    int port = 9000;
    mapper_router rt = mapper_router_new(host, port);
    mdev_add_router(md, rt);
    printf("Router to %s:%d added.\n", host, port);

    mapper_router_add_mapping(rt, sig, "/mapped1");
    mapper_router_add_mapping(rt, sig, "/mapped2");

    printf("Polling device..\n");
    int i;
    for (i=0; i<10; i++) {
        mdev_poll(md, 500);
        printf("Updating signal %s to %f\n", sig->name, (i*1.0f));
        msig_update_scalar(sig, (mval)(i*1.0f));
    }

    mdev_remove_router(md, rt);
    printf("Router removed.\n");

    mdev_free(md);
    return 0;

  error:
    if (md) mdev_free(md);
    return 1;
}

int main()
{
    int result = test_admin();
    if (result) {
        printf("Test FAILED.\n");
        return 1;
    }

    result = test_controller();
    if (result) {
        printf("Test FAILED.\n");
        return 1;
    }

    printf("Test PASSED.\n");
    return 0;
}
