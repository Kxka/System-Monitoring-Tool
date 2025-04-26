#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <ctype.h>
#include <math.h>

#define DEFAULT_SAMPLES 20
#define DEFAULT_TDELAY 500000
#define MIN_SAMPLES 1
#define MIN_TDELAY 10000 // 10ms
#define GIGABYTE 1073741824
#define MEMORY_HEIGHT 12
#define CPU_HEIGHT 10
#define BUFFER 500

void parse_arguments(int argc, char *argv[], int *samples, int *tdelay, int *show_memory, int *show_cpu, int *show_cores);
void display_memory_usage(int sample_num);
void clear_screen();
void move_cursor_top();
void shift_cursor(int rows, int cols);
void move_cursor_position(int row, int col);
void draw_graph_outline(int width, int height);
void draw_memory_graph(int *samples);
void draw_cpu_graph(int *samples, int show_memory);
void display_cpu_usage(int sample_num, int show_memory, long *prev_cpu_idle, long *prev_cpu_total);
float calculate_cpu_usage(long prev_total, long prev_idle, long new_total, long new_idle);
void get_cpu_usage(long *new_total, long *new_idle);
void getCpuInfo(int *num_cores, float *max_frequency);
void display_cores();
void printsquare();

int main(int argc, char *argv[])
{
    int samples = DEFAULT_SAMPLES;
    int tdelay = DEFAULT_TDELAY;
    int show_memory, show_cpu, show_cores;

    parse_arguments(argc, argv, &samples, &tdelay, &show_memory, &show_cpu, &show_cores);
    clear_screen();
    move_cursor_top();
    printf("Nbr of samples: %d -- every %d microSecs (%.3f secs)\n\n", samples, tdelay, (float)tdelay / 1000000);

    // If memory or CPU is shown, a loop is needed to update the graphs
    if (show_memory || show_cpu)
    {
        if (show_memory)
            draw_memory_graph(&samples);
        if (show_cpu)
            draw_cpu_graph(&samples, show_memory);

        long prev_cpu_idle = 0, prev_cpu_total = 0;     // stores prev total and idle cpu time
        get_cpu_usage(&prev_cpu_total, &prev_cpu_idle); // find the first cpu usage snapshot
        usleep(tdelay);

        // continously update the graphs by looping through the samples
        for (int sample_num = 0; sample_num < samples; sample_num++)
        {
            if (show_memory)
                display_memory_usage(sample_num);
            if (show_cpu)
                display_cpu_usage(sample_num, show_memory, &prev_cpu_idle, &prev_cpu_total);

            fflush(stdout); // prevents output from not updating
            usleep(tdelay); // pause for tdelay microseconds
        }
    }

    // display core info
    if (show_cores)
        display_cores();
    return 0;
}

void parse_arguments(int argc, char *argv[], int *samples, int *tdelay, int *show_memory, int *show_cpu, int *show_cores)
{
    *samples = DEFAULT_SAMPLES;
    *tdelay = DEFAULT_TDELAY;
    *show_memory = 0;
    *show_cpu = 0;
    *show_cores = 0;

    int arg_index = 1; // Start checking from the first argument

    // Check if the 1st argument is a number
    if (arg_index < argc && isdigit(argv[arg_index][0]))
    {
        *samples = atoi(argv[arg_index]);
        arg_index++;
        // Check if the 2nd argument is also a number given the 1st argument is a number
        if (arg_index < argc && isdigit(argv[arg_index][0]))
        {
            *tdelay = atoi(argv[arg_index]);
            arg_index++;
        }
    }

    // Process the rest of the arguments
    for (; arg_index < argc; arg_index++)
    {
        if (strcmp(argv[arg_index], "--memory") == 0)
        {
            *show_memory = 1;
        }
        else if (strcmp(argv[arg_index], "--cpu") == 0)
        {
            *show_cpu = 1;
        }
        else if (strcmp(argv[arg_index], "--cores") == 0)
        {
            *show_cores = 1;
        }
        else if (strncmp(argv[arg_index], "--samples=", 10) == 0)
        {
            *samples = atoi(argv[arg_index] + 10);
        }
        else if (strncmp(argv[arg_index], "--tdelay=", 9) == 0)
        {
            *tdelay = atoi(argv[arg_index] + 9);
        }
        else
        {
            printf("Error: Argument format wrong\n");
            exit(1);
        }
    }

    // If no arguments are provided, show all
    if (!*show_memory && !*show_cpu && !*show_cores)
    {
        *show_memory = 1;
        *show_cpu = 1;
        *show_cores = 1;
    }

    // Check for invalid Samples and tdelay values
    if (*samples < MIN_SAMPLES)
    {
        printf("Error: samples must be at least %d.\n", MIN_SAMPLES);
        exit(1);
    }
    else if (*tdelay < MIN_TDELAY)
    {
        printf("Error: tdelay must be at least %d.\n", MIN_TDELAY);
        exit(1);
    }
}

// Update Memory graph where sample_num is the current sample number
void display_memory_usage(int sample_num)
{
    struct sysinfo info;
    if (sysinfo(&info) == 0)
    { // store sys info in info, success reutrn 0
        double total_ram = (info.totalram * info.mem_unit) / (double)GIGABYTE;
        double free_ram = (info.freeram * info.mem_unit) / (double)GIGABYTE;
        double used_ram = total_ram - free_ram;
        int used_ram_percent = round((used_ram / total_ram) * 12); // round is used -lm flag needed

        // default position as memory is always first if shown
        move_cursor_position(16, 9);
        shift_cursor(-used_ram_percent, sample_num);
        printf("#");

        // Print memory used top of graph
        move_cursor_position(3, 11);
        printf("%.2f", used_ram);

        // Print memory total at left of graph
        move_cursor_position(4, 1);
        printf("%.2f", total_ram);

        // Move cursor to default position
        move_cursor_position(17, 1);
    }
    else
    { // Error handling
        clear_screen();
        move_cursor_top();
        printf("sysinfo failed, cannot retrieve memory usage\n");
        exit(1);
    }
}

void clear_screen()
{
    printf("\033[2J");
}

void move_cursor_top()
{
    printf("\033[H");
}

void move_cursor_pos(int row, int col)
{
    printf("\033[%d;%dH", row, col);
}

// Move cursor by 'rows' and 'cols', rows positive moves down, negative moves up, cols positive moves right, negative moves left
void shift_cursor(int rows, int cols)
{
    if (rows < 0)
    {
        printf("\033[%dA", -rows); // Move up by 'rows' lines
    }
    else if (rows > 0)
    {
        printf("\033[%dB", rows); // Move down by 'rows' lines
    }

    if (cols < 0)
    {
        printf("\033[%dD", -cols); // Move left by 'cols' columns
    }
    else if (cols > 0)
    {
        printf("\033[%dC", cols); // Move right by 'cols' columns
    }
}

// Draws the graph outline given the width and height
void draw_graph_outline(int width, int height)
{
    for (int row = height; row > 0; row--)
    {
        printf("       |\n"); // Y-axis
    }
    printf("       "); // empty spaces before horizotal line

    int column = width;
    column = column + 1;
    for (; column > 0; column--)
    {
        printf("─"); // X-axis
    }
    printf("\n");
}

void draw_memory_graph(int *samples)
{
    printf("v Memory       GB\n");
    draw_graph_outline(*samples, MEMORY_HEIGHT);
    shift_cursor(-13, 0);
    printf("     GB");
    shift_cursor(12, -8);
    printf("  0 GB");
    printf("\033[F\n\n");
}

void draw_cpu_graph(int *samples, int show_memory)
{

    // if memory is shown, move cursor to correct position as CPU is shown after memory
    if (show_memory == 1)
    {
        move_cursor_position(1, 1);
        shift_cursor(17, 0);
    }

    printf("v CPU\n");
    draw_graph_outline(*samples, CPU_HEIGHT);
    shift_cursor(-1, -8);
    printf("    0%%");
    shift_cursor(-10, -5);
    printf("  100%%");
    printf("\033[F\n\n");
}

void display_cpu_usage(int sample_num, int show_memory, long *prev_cpu_idle, long *prev_cpu_total)
{

    // Get new CPU usage values
    long new_total = 0, new_idle = 0;
    get_cpu_usage(&new_total, &new_idle);

    // Compute differences
    float cpu_usage = calculate_cpu_usage(*prev_cpu_total, *prev_cpu_idle, new_total, new_idle); // returns as percentage

    // Check for error
    if (cpu_usage < 0)
    {
        clear_screen();
        move_cursor_top();
        printf("Error: CPU usage calculation failed\n");
        exit(1);
    }

    // Convert CPU usage to a value out of 10
    int cpu_usage_value = round(cpu_usage / 10);

    // Update cpu graph. Chooses cursor position depending on whether memory is shown
    if (show_memory == 0)
    { // memory not shown
        move_cursor_position(14, 9);
        shift_cursor(-cpu_usage_value, sample_num);
        printf(":");
        move_cursor_position(3, 8);
        printf("%.2f%%", cpu_usage);
        move_cursor_position(15, 1);
    }
    else
    {
        move_cursor_position(29, 9); // memory shown
        shift_cursor(-cpu_usage_value, sample_num);
        printf(":");
        move_cursor_position(18, 8);
        printf("%.2f%%", cpu_usage);
        move_cursor_position(30, 1);
    }

    // Update previous values for next sample
    *prev_cpu_total = new_total;
    *prev_cpu_idle = new_idle;
}

void get_cpu_usage(long *new_total, long *new_idle)
{

    // Open /proc/stat to read CPU usage
    FILE *file = fopen("/proc/stat", "r");
    if (!file)
    {
        clear_screen();
        move_cursor_top();
        printf("/proc/stat not working\n");
        exit(1);
    }

    // Read the first line of /proc/stat
    char buffer[BUFFER]; // Assume buffer is large enough
    fgets(buffer, sizeof(buffer), file);
    fclose(file);

    // CPU usage always has same format with increasing values, store them in long varables
    long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
    user = nice = system = idle = iowait = irq = softirq = steal = guest = guest_nice = 0;
    sscanf(buffer, "cpu %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);

    // Total time is the sum of all times, including guest (virtual CPU) time
    *new_total = user + nice + system + idle + iowait + irq + softirq + steal + guest + guest_nice;

    // Idle time is the sum of idle and iowait
    *new_idle = idle + iowait;
}

float calculate_cpu_usage(long prev_total, long prev_idle, long new_total, long new_idle)
{

    // Calculate the differences in total and aidle time
    long total_diff = new_total - prev_total;
    long idle_diff = new_idle - prev_idle;

    // Returns the CPU usage as a float percentage
    return 100.0 * (1.0 - (((float)idle_diff / (float)total_diff)));
}

void getCpuInfo(int *num_cores, float *max_frequency)
{
    FILE *fp;
    char line[BUFFER];

    // Check the number of cores
    fp = fopen("/proc/cpuinfo", "r");
    if (fp == NULL)
    {
        clear_screen();
        move_cursor_top();
        printf("Error: /proc/cpuinfo\n");
        exit(1);
    }

    // Read the number of cores by counting the number of lines starting with "processor"
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "processor", 9) == 0)
        {
            (*num_cores)++; // Increment the number of cores
        }
    }
    fclose(fp);

    // Get the max frequency for the first core (Assume the same for all cores)
    fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
    if (fp == NULL)
    {
        clear_screen();
        move_cursor_top();
        printf("Error: Max freq of cores cannot be retrieved\n");
        exit(1);
    }

    // Read the max frequency directly into max_frequency as a float
    fscanf(fp, "%f", max_frequency);
    fclose(fp);
}

void display_cores()
{
    int num_cores = 0;
    float max_frequency = 0;
    getCpuInfo(&num_cores, &max_frequency);
    printf("\nv Number of Cores: %d @ %.2f GHz\n", num_cores, max_frequency / 1000000.0); // max frequency convert toGHz

    // Creates new lines to prevent overlapping with the previous graphs
    printf("\n\n\n");
    shift_cursor(-3, 0);

    // Print the squares for each core
    for (int i = 1; i <= num_cores; i++)
    {
        printsquare();

        // Print a newline after every 4 outputs while prevent overlapping with the previous graphs
        if (i % 4 == 0)
        {
            printf("\n\n\n\n\n\n");
            shift_cursor(-3, 0);
        }
    }

    // Print a final newline if the last line has less than 4 outputs
    if (num_cores % 4 != 0)
    {
        printf("\n\n\n");
    }
}

void printsquare()
{
    printf("+---+ ");
    shift_cursor(1, -6);
    printf("|   | ");
    shift_cursor(1, -6);
    printf("+---+ ");
    shift_cursor(-2, 1);
}