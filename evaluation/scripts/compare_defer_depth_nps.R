file_part_one <- "pos1_16384_abdada_d10_dd"
file_part_two <- ""

variable_parts <- c("1", "2", "3")

# Initialize an empty data frame to store the combined data
combined_data <- data.frame()

# Loop over the list of file names and read in the data
for (var in variable_parts) {
  data <- read.table(paste0(file_part_one, var, file_part_two, ".txt"), header = TRUE)
  
  # Filter the data to only include ply = 10
  data <- data[data$ply == 10, ]

  data_agg <- aggregate(nps ~ num_thr, data, mean)

  # Add a column for the file name and append to existing data
  data_agg$var <- var
  combined_data <- rbind(combined_data, data_agg)
}

# Normalize the nps data by dividing by the maximum value at num_thr = 1
combined_data$nps_norm <- combined_data$nps / max(combined_data$nps[combined_data$num_thr == 1])

max_threads = max(combined_data$num_thr)

library(ggplot2)

pdf("abdada_defer_depth_nps.pdf")

ggplot(combined_data, aes(x = num_thr, y = nps_norm, group = var)) +
  geom_line(aes(color = var)) +
  scale_x_continuous(breaks = c(1, max_threads / 4, max_threads / 2, max_threads * 3 / 4, max_threads)) +
  scale_color_discrete(name = "Defer Depth") +
  labs(x = "Number of Threads", y = "Speedup (Normalized NPS)")

dev.off()

