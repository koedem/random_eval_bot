file_name <- "pos1_16384_lazy_d10"

data <- read.table(paste0(file_name, ".txt"), header = TRUE)

# Aggregate data by ply, num_thr, and calculate the average nps
data_agg <- aggregate(nps ~ ply + num_thr, data, mean)

# Normalize nps by dividing by nps at num_thr = 1
data_norm <- merge(data_agg, subset(data_agg, num_thr == 1), by = "ply", suffixes = c("", "_1"))
data_norm$nps_norm <- data_norm$nps / data_norm$nps_1

data_trimmed <- subset(data_norm, ply >= 5)
max_threads = max(data_norm$num_thr)

library(ggplot2)

pdf(paste0(file_name, "_nps.pdf"))
ggplot(data_trimmed, aes(x=num_thr, y=nps_norm, group=ply)) +
  geom_line(aes(color=ply)) +
  scale_color_viridis_c() +
  scale_x_continuous(breaks = seq(0, max(data_norm$num_thr), by=max(data_norm$num_thr)/4)) +
  labs(title = "Speedup (NPS) vs Number of Threads",
       x = "Number of Threads",
       y = "Speedup (Normalized NPS)",
       color = "Ply")

dev.off()
