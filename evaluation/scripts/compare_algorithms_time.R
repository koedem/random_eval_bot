file_part_one <- "pos1_16384_"
file_part_two <- "_d10"

variable_parts <- c("lazy", "abdada", "simple-abdada")

# Initialize an empty data frame to store the combined data
combined_data <- data.frame()

# Loop over the list of file names and read in the data
for (var in variable_parts) {
  data <- read.table(paste0(file_part_one, var, file_part_two, ".txt"), header = TRUE)
  
  # Filter the data to only include ply = 10
  data <- data[data$ply == 10, ]

  data_agg <- aggregate(time ~ num_thr, data, mean)

  # Add a column for the file name and append to existing data
  data_agg$var <- var
  combined_data <- rbind(combined_data, data_agg)
}

# Normalize the time data by dividing by the maximum value at num_thr = 1
combined_data$time_norm <-  min(combined_data$time[combined_data$num_thr == 1]) / combined_data$time

max_threads = max(combined_data$num_thr)

library(ggplot2)

pdf("compare_algorithms_time.pdf")

ggplot(combined_data, aes(x = num_thr, y = time_norm, group = var)) +
  geom_line(aes(color = var)) +
  scale_x_continuous(breaks = c(1, max_threads / 4, max_threads / 2, max_threads * 3 / 4, max_threads)) +
  scale_color_discrete(name = "Algorithm") +
  labs(x = "Number of Threads", y = "Speedup (Normalized Time)")

dev.off()

