# R script for plotting acpr performance
# Load packages
require(readr)    # for read_csv()
require(dplyr)    # for arrange(), mutate(), filter(), group_by(), summarize()
require(tidyr)    # for unnest()
require(purrr)    # for map()
require(ggplot2)  # for ggplot()
require(reshape2) # for melt()

####### Load/Format data #######
# Read all laptop datafiles into single dataframe for ggplot2
laptop_data_path <- "../test/data/laptop/"
laptop_dataFiles <- list.files(path=laptop_data_path, pattern=".csv")

laptop_df <- data_frame(Test = unlist(strsplit(laptop_dataFiles, split=".csv"))) %>%
           mutate(file_contents = map(laptop_dataFiles, ~ read_csv(file.path(laptop_data_path, .), skip=1)))  %>%
           unnest %>%
           melt(id.vars = c("Test", "Trial"), value.name = "Time", variable.name = "Program")

# Read all csres datafiles into single dataframe for ggplot2
csres_data_path <- "../test/data/csres/"
csres_dataFiles <- list.files(path=csres_data_path, pattern=".csv")

csres_df <- data_frame(Test = unlist(strsplit(csres_dataFiles, split=".csv"))) %>%
          mutate(file_contents = map(csres_dataFiles, ~ read_csv(file.path(csres_data_path, .), skip=1)))  %>%
          unnest %>%
          melt(id.vars = c("Test", "Trial"), value.name = "Time", variable.name = "Program")

# Read all macbook datafiles into single dataframe for ggplot2
macbook_data_path <- "../test/data/macbook/"
macbook_dataFiles <- list.files(path=macbook_data_path, pattern=".csv")

macbook_df <- data_frame(Test = unlist(strsplit(macbook_dataFiles, split=".csv"))) %>%
              mutate(file_contents = map(macbook_dataFiles, ~ read_csv(file.path(macbook_data_path, .), skip=1)))  %>%
              unnest %>%
              melt(id.vars = c("Test", "Trial"), value.name = "Time", variable.name = "Program")


# Extract different datasets
laptop_linux_df <- laptop_df %>% filter(grepl('linux*', Test)) %>%
                              arrange(Test, Trial, Program)
laptop_lubuntu_df <- laptop_df %>% filter(grepl('lubuntu*', Test)) %>%
                                   arrange(Test, Trial, Program)
csres_linux_df <- csres_df %>% filter(grepl('linux*', Test)) %>%
                               arrange(Test, Trial, Program)
csres_lubuntu_df <- csres_df %>% filter(grepl('lubuntu*', Test)) %>%
                                 arrange(Test, Trial, Program)
macbook_linux_df <- macbook_df %>% filter(grepl('linux*', Test)) %>%
                                   arrange(Test, Trial, Program)
macbook_lubuntu_df <- macbook_df %>% filter(grepl('lubuntu*', Test)) %>%
                                     arrange(Test, Trial, Program)

# Remove prefix from datasets
laptop_linux_df$Test <- sapply(strsplit(laptop_linux_df$Test, split='linux_', fixed=TRUE), function(x) (x[2]))
laptop_lubuntu_df$Test <- sapply(strsplit(laptop_lubuntu_df$Test, split='lubuntu_', fixed=TRUE), function(x) (x[2]))
csres_linux_df$Test <- sapply(strsplit(csres_linux_df$Test, split='linux_', fixed=TRUE), function(x) (x[2]))
csres_lubuntu_df$Test <- sapply(strsplit(csres_lubuntu_df$Test, split='lubuntu_', fixed=TRUE), function(x) (x[2]))
macbook_linux_df$Test <- sapply(strsplit(macbook_linux_df$Test, split='linux_', fixed=TRUE), function(x) (x[2]))
macbook_lubuntu_df$Test <- sapply(strsplit(macbook_lubuntu_df$Test, split='lubuntu_', fixed=TRUE), function(x) (x[2]))

# Reorder tests to start at default
laptops_lubuntu_order <- c("default", "fallocate-readahead", "128K", "256K", "512K", "10ms", "1ms", "0ms")
laptops_linux_order <- c("default", "threshold4K", "threshold8K", "32K", "16K", "cb16", "cb32", "10ms", "1ms", "0ms")
csres_lubuntu_order <- c(laptops_lubuntu_order, "default_nosync", "fallocate-readahead_nosync")
csres_linux_order <- laptops_linux_order

laptop_linux_df$Test <- factor(laptop_linux_df$Test, laptops_linux_order)
laptop_lubuntu_df$Test <- factor(laptop_lubuntu_df$Test, laptops_lubuntu_order)
csres_linux_df$Test <- factor(csres_linux_df$Test, csres_linux_order)
csres_lubuntu_df$Test <- factor(csres_lubuntu_df$Test, csres_lubuntu_order)
macbook_linux_df$Test <- factor(macbook_linux_df$Test, laptops_linux_order)
macbook_lubuntu_df$Test <- factor(macbook_lubuntu_df$Test, laptops_lubuntu_order)


# Constants in MB
linux_size = 711
lubuntu_size = 880

# Compute summary statistics
laptop_linux_stats <- laptop_linux_df %>%
                      group_by(Test, Program) %>% 
                      summarize(Mean = mean(Time), Std = sd(Time)) %>%
                      mutate(Thru = linux_size / Mean)
laptop_lubuntu_stats <- laptop_lubuntu_df %>% 
                        group_by(Test, Program) %>% 
                        summarize(Mean = mean(Time), Std = sd(Time)) %>%
                        mutate(Thru = lubuntu_size / Mean)
csres_linux_stats <- csres_linux_df %>%
                     group_by(Test, Program) %>% 
                     summarize(Mean = mean(Time), Std = sd(Time)) %>%
                     mutate(Thru = linux_size / Mean)
csres_lubuntu_stats <- csres_lubuntu_df %>% 
                       group_by(Test, Program) %>% 
                       summarize(Mean = mean(Time), Std = sd(Time)) %>%
                       mutate(Thru = lubuntu_size / Mean)
macbook_linux_stats <- macbook_linux_df %>%
                       group_by(Test, Program) %>% 
                       summarize(Mean = mean(Time), Std = sd(Time)) %>%
                       mutate(Thru = linux_size / Mean)
macbook_lubuntu_stats <- macbook_lubuntu_df %>% 
                         group_by(Test, Program) %>% 
                         summarize(Mean = mean(Time), Std = sd(Time)) %>%
                         mutate(Thru = lubuntu_size / Mean)

####### Barplots with error bars #######
# Plot laptop linux data
p <- laptop_linux_stats %>% ggplot(aes(x = Test, y = Mean, fill=Program)) +  
                     geom_bar(stat="identity", position = position_dodge(0.75)) +
                     geom_errorbar(aes(ymin=Mean - Std, ymax=Mean + Std), width=.2,position=position_dodge(0.75)) +
                     labs(title="ACPR vs CPR on Thinkpad", subtitle="Copying Linux 4.4 kernel") +
                     theme_bw() +
                     theme(axis.text.x=element_text(angle=90,hjust=1))
ggsave(filename = "Laptop_Linux_Barplot.pdf", p)

# Plot laptop lubuntu data
p <- laptop_lubuntu_stats %>% ggplot(aes(x = Test, y = Mean, fill=Program)) +  
                       geom_bar(stat="identity", position = position_dodge(0.75)) +
                       geom_errorbar(aes(ymin=Mean - Std, ymax=Mean + Std), width=.2,position=position_dodge(0.75)) +
                       labs(title="ACPR vs CPR on Thinkpad", subtitle="Copying Lubuntu ISO") +
                       theme_bw() +
                       theme(axis.text.x=element_text(angle=90,hjust=1))
ggsave(filename = "Laptop_Lubuntu_Barplot.pdf", p)

# Plot csres linux data
p <- csres_linux_stats %>% ggplot(aes(x = Test, y = Mean, fill=Program)) +  
                    geom_bar(stat="identity", position = position_dodge(0.75)) +
                    geom_errorbar(aes(ymin=Mean - Std, ymax=Mean + Std), width=.2,position=position_dodge(0.75)) +
                    labs(title="ACPR vs CPR on Zerberus", subtitle="Copying Linux 4.4 kernel") +
                    theme_bw() +
                    theme(axis.text.x=element_text(angle=90,hjust=1))
ggsave(filename = "CSRES_Linux_Barplot.pdf", p)

# Plot csres lubuntu data
p <- csres_lubuntu_stats %>% ggplot(aes(x = Test, y = Mean, fill=Program)) +  
                      geom_bar(stat="identity", position = position_dodge(0.75)) +
                      geom_errorbar(aes(ymin=Mean - Std, ymax=Mean + Std), width=.2,position=position_dodge(0.75)) +
                      labs(title="ACPR vs CPR on Zerberus", subtitle="Copying Lubuntu ISO") +
                      theme_bw() +
                      theme(axis.text.x=element_text(angle=90,hjust=1))
ggsave(filename = "CSRES_Lubuntu_Barplot.pdf", p)

# Plot macbook linux data
p <- macbook_linux_stats %>% ggplot(aes(x = Test, y = Mean, fill=Program)) +  
                             geom_bar(stat="identity", position = position_dodge(0.75)) +
                             geom_errorbar(aes(ymin=Mean - Std, ymax=Mean + Std), width=.2,position=position_dodge(0.75)) +
                             labs(title="ACPR vs CPR on Macbook Pro", subtitle="Copying Linux 4.4 kernel") +
                             theme_bw() +
                             theme(axis.text.x=element_text(angle=90,hjust=1))
ggsave(filename = "Macbook_Linux_Barplot.pdf", p)

# Plot macbook lubuntu data
p <- macbook_lubuntu_stats %>% ggplot(aes(x = Test, y = Mean, fill=Program)) +  
                               geom_bar(stat="identity", position = position_dodge(0.75)) +
                               geom_errorbar(aes(ymin=Mean - Std, ymax=Mean + Std), width=.2,position=position_dodge(0.75)) +
                               labs(title="ACPR vs CPR on Macbook Pro", subtitle="Copying Lubuntu ISO") +
                               theme_bw() +
                               theme(axis.text.x=element_text(angle=90,hjust=1))
ggsave(filename = "Macbook_Lubuntu_Barplot.pdf", p)

###### Throughput barplots ######
# Plot laptop linux data
laptop_linux_stats %>% ggplot(aes(x = Test, y = Thru, fill=Program)) +  
                            geom_bar(stat="identity", position = position_dodge(0.75)) +
                            labs(title="ACPR vs CPR on Thinkpad", subtitle="Copying Linux 4.4 kernel", y = "Throughput (MB/s)") +
                            theme_bw() +
                            theme(axis.text.x=element_text(angle=90,hjust=1))

# Plot laptop lubuntu data
laptop_lubuntu_stats %>% ggplot(aes(x = Test, y = Thru, fill=Program)) +  
                              geom_bar(stat="identity", position = position_dodge(0.75)) +
                              labs(title="ACPR vs CPR on Thinkpad", subtitle="Copying Lubuntu ISO", y = "Troughput (MB/s)") +
                              theme_bw() +
                              theme(axis.text.x=element_text(angle=90,hjust=1))

# Plot csres linux data
csres_linux_stats %>% ggplot(aes(x = Test, y = Thru, fill=Program)) +  
                           geom_bar(stat="identity", position = position_dodge(0.75)) +
                           labs(title="ACPR vs CPR on Zerberus", subtitle="Copying Linux 4.4 kernel", y = "Throughput (MB/s)") +
                           theme_bw() +
                           theme(axis.text.x=element_text(angle=90,hjust=1))

# Plot csres lubuntu data
csres_lubuntu_stats %>% ggplot(aes(x = Test, y = Thru, fill=Program)) +  
                             geom_bar(stat="identity", position = position_dodge(0.75)) +
                             labs(title="ACPR vs CPR on Zerberus", subtitle="Copying Lubuntu ISO", y = "Troughput (MB/s)") +
                             theme_bw() +
                             theme(axis.text.x=element_text(angle=90,hjust=1))

# Plot macbook linux data
macbook_linux_stats %>% ggplot(aes(x = Test, y = Thru, fill=Program)) +  
                             geom_bar(stat="identity", position = position_dodge(0.75)) +
                             labs(title="ACPR vs CPR on Macbook Pro", subtitle="Copying Linux 4.4 kernel", y = "Throughput (MB/s)") +
                             theme_bw() +
                             theme(axis.text.x=element_text(angle=90,hjust=1))

# Plot macbook lubuntu data
macbook_lubuntu_stats %>% ggplot(aes(x = Test, y = Thru, fill=Program)) +  
                               geom_bar(stat="identity", position = position_dodge(0.75)) +
                               labs(title="ACPR vs CPR on Macbook Pro", subtitle="Copying Lubuntu ISO", y = "Troughput (MB/s)") +
                               theme_bw() +
                               theme(axis.text.x=element_text(angle=90,hjust=1))

####### Boxplots #######
# Plot laptop linux data
laptop_linux_df %>% ggplot(aes(x=Test, y=Time, fill=Program)) + 
                  geom_boxplot() +
                  stat_summary(fun.y=mean, geom="point", shape=5, size=3) +
                  facet_wrap(~Program) +
                  labs(title="ACPR vs CPR on Thinkpad", subtitle="Copying Linux 4.4 kernel") +
                  theme_bw() +
                  theme(axis.text.x=element_text(angle=90,hjust=1))

# Plot laptop lubuntu data
laptop_lubuntu_df %>% ggplot(aes(x=Test, y=Time, fill=Program)) + 
                    geom_boxplot() +
                    stat_summary(fun.y=mean, geom="point", shape=5, size=3) +
                    facet_wrap(~Program) +
                    labs(title="ACPR vs CPR on Thinkpad", subtitle="Copying Lubuntu ISO") +
                    theme_bw() +
                    theme(axis.text.x=element_text(angle=90,hjust=1))

# Plot csres linux data
csres_linux_df %>% ggplot(aes(x=Test, y=Time, fill=Program)) + 
                 geom_boxplot() +
                 stat_summary(fun.y=mean, geom="point", shape=5, size=3) +
                 facet_wrap(~Program) +
                 labs(title="ACPR vs CPR on Zerberus", subtitle="Copying Linux 4.4 kernel") +
                 theme_bw() +
                 theme(axis.text.x=element_text(angle=90,hjust=1))

# Plot csres lubuntu data
csres_lubuntu_df %>% ggplot(aes(x=Test, y=Time, fill=Program)) + 
                   geom_boxplot() +
                   stat_summary(fun.y=mean, geom="point", shape=5, size=3) +
                   facet_wrap(~Program) +
                   labs(title="ACPR vs CPR on Zerberus", subtitle="Copying Lubuntu ISO") +
                   theme_bw() +
                   theme(axis.text.x=element_text(angle=90,hjust=1))

# Plot macbook linux data
macbook_linux_df %>% ggplot(aes(x=Test, y=Time, fill=Program)) + 
                     geom_boxplot() +
                     stat_summary(fun.y=mean, geom="point", shape=5, size=3) +
                     facet_wrap(~Program) +
                     labs(title="ACPR vs CPR on Macbook Pro", subtitle="Copying Linux 4.4 kernel") +
                     theme_bw() +
                     theme(axis.text.x=element_text(angle=90,hjust=1))

# Plot macbook lubuntu data
macbook_lubuntu_df %>% ggplot(aes(x=Test, y=Time, fill=Program)) + 
                       geom_boxplot() +
                       stat_summary(fun.y=mean, geom="point", shape=5, size=3) +
                       facet_wrap(~Program) +
                       labs(title="ACPR vs CPR on Macbook Pro", subtitle="Copying Lubuntu ISO") +
                       theme_bw() +
                       theme(axis.text.x=element_text(angle=90,hjust=1))

####### Trial plots #######
# Plot all for laptop linux
laptop_linux_df %>% ggplot(aes(x=Trial, y=Time, fill=Program, color=Program)) + 
                  geom_point() + 
                  facet_wrap(~Test) +
                  labs(title="ACPR vs CPR on Thinkpad", subtitle="Copying Linux 4.4 kernel") +
                  theme_minimal()

# Plot all for laptop lubuntu
laptop_lubuntu_df %>% ggplot(aes(x=Trial, y=Time, fill=Program, color=Program)) + 
                    geom_point() + 
                    facet_wrap(~Test) +
                    labs(title="ACPR vs CPR on Thinkpad", subtitle="Copying Lubuntu ISO") +
                    theme_minimal()

# Plot all for csres linux
csres_linux_df %>% ggplot(aes(x=Trial, y=Time, fill=Program, color=Program)) + 
                 geom_point() + 
                 facet_wrap(~Test) +
                 labs(title="ACPR vs CPR on Zerberus", subtitle="Copying Linux 4.4 kernel") +
                 theme_minimal()

# Plot all for csres lubuntu
csres_lubuntu_df %>% ggplot(aes(x=Trial, y=Time, fill=Program, color=Program)) + 
                   geom_point() + 
                   facet_wrap(~Test) +
                   labs(title="ACPR vs CPR on Zerberus", subtitle="Copying Lubuntu ISO") +
                   theme_minimal()

# Plot all for macbook linux
macbook_linux_df %>% ggplot(aes(x=Trial, y=Time, fill=Program, color=Program)) + 
                     geom_point() + 
                     facet_wrap(~Test) +
                     labs(title="ACPR vs CPR on Macbook Pro", subtitle="Copying Linux 4.4 kernel") +
                     theme_minimal()

# Plot all for macbook lubuntu
macbook_lubuntu_df %>% ggplot(aes(x=Trial, y=Time, fill=Program, color=Program)) + 
                       geom_point() + 
                       facet_wrap(~Test) +
                       labs(title="ACPR vs CPR on Macbook Pro", subtitle="Copying Lubuntu ISO") +
                       theme_minimal()