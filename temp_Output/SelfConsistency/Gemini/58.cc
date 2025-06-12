#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBbrSimulation");

int main(int argc, char* argv[]) {
  // Enable logging
  LogComponentEnable("TcpBbrSimulation", LOG_LEVEL_INFO);

  // Set the simulation time
  Time::SetResolution(Time::NS);
  double simulationTime = 100; // seconds

  // Create nodes
  NodeContainer nodes;
  nodes.Create(4); // Sender, R1, R2, Receiver

  // Name the nodes
  Names::Add("Sender", nodes.Get(0));
  Names::Add("R1", nodes.Get(1));
  Names::Add("R2", nodes.Get(2));
  Names::Add("Receiver", nodes.Get(3));

  // Create point-to-point helper
  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
  p2pHelper.SetChannelAttribute("Delay", StringValue("5ms"));

  // Create bottleneck link helper
  PointToPointHelper bottleneckHelper;
  bottleneckHelper.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  bottleneckHelper.SetChannelAttribute("Delay", StringValue("10ms"));

  // Install network devices
  NetDeviceContainer senderR1Devices = p2pHelper.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer senderR2Devices = p2pHelper.Install(nodes.Get(0), nodes.Get(2));  // Extra link
  NetDeviceContainer R1R2Devices = bottleneckHelper.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer R2ReceiverDevices = p2pHelper.Install(nodes.Get(2), nodes.Get(3));

  // Install internet stack
  InternetStackHelper stackHelper;
  stackHelper.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper addressHelper;
  addressHelper.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer senderR1Interfaces = addressHelper.Assign(senderR1Devices);

  addressHelper.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer senderR2Interfaces = addressHelper.Assign(senderR2Devices);

  addressHelper.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer R1R2Interfaces = addressHelper.Assign(R1R2Devices);

  addressHelper.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer R2ReceiverInterfaces = addressHelper.Assign(R2ReceiverDevices);

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Configure TCP BBR
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));

  // Create applications (Traffic)
  uint16_t port = 50000;
  BulkSendHelper source("ns3::TcpSocketFactory",
                         InetSocketAddress(R2ReceiverInterfaces.GetAddress(1), port)); // destination is receiver
  source.SetAttribute("SendSize", UintegerValue(1448));
  ApplicationContainer sourceApps = source.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(simulationTime));

  // Create a sink application on the receiver
  PacketSinkHelper sink("ns3::TcpSocketFactory",
                       InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(nodes.Get(3));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(simulationTime));

  // Configure Tracing
  p2pHelper.EnablePcapAll("TcpBbrSimulation");
  bottleneckHelper.EnablePcapAll("TcpBbrSimulation");

  // Congestion Window trace
  Gnuplot congestionWindowPlot("congestion_window.plt");
  congestionWindowPlot.SetTitle("TCP BBR Congestion Window");
  congestionWindowPlot.SetLegend("Time (s)", "Congestion Window (segments)");
  Gnuplot2dDataset congestionWindowDataset;
  congestionWindowDataset.SetTitle("Congestion Window");
  congestionWindowDataset.SetStyle(Gnuplot2dDataset::LINES);

  // Sender Throughput trace
  Gnuplot throughputPlot("throughput.plt");
  throughputPlot.SetTitle("Sender Throughput");
  throughputPlot.SetLegend("Time (s)", "Throughput (Mbps)");
  Gnuplot2dDataset throughputDataset;
  throughputDataset.SetTitle("Throughput");
  throughputDataset.SetStyle(Gnuplot2dDataset::LINES);

  // Queue Size trace (Bottleneck Link)
  Gnuplot queueSizePlot("queue_size.plt");
  queueSizePlot.SetTitle("Bottleneck Queue Size");
  queueSizePlot.SetLegend("Time (s)", "Queue Size (packets)");
  Gnuplot2dDataset queueSizeDataset;
  queueSizeDataset.SetTitle("Queue Size");
  queueSizeDataset.SetStyle(Gnuplot2dDataset::LINES);

  // Congestion Window callback
  Simulator::Schedule(Seconds(0.1), [&, senderR1Interfaces]() {
    Ptr<TcpSocket> socket = DynamicCast<TcpSocket>(
        GetObject<Node>(0)->GetApplication(0)->GetObject<BulkSendApplication>(0)->GetSocket());
    if (socket) {
      congestionWindowDataset.Add(Simulator::Now().GetSeconds(), socket->GetCongestionWindow());
    }
  });

  // Throughput callback
  uint64_t lastTotalRx = 0;
  Simulator::Schedule(Seconds(0.1), [&, sinkApps, &lastTotalRx]() {
      Ptr<Application> app = sinkApps.Get(0);
      Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
      uint64_t totalRx = sink->GetTotalRx();
      double throughput = (totalRx - lastTotalRx) * 8.0 / 1000000; // in Mbps
      throughputDataset.Add(Simulator::Now().GetSeconds(), throughput);
      lastTotalRx = totalRx;
  });

  // Queue size callback
  Simulator::Schedule(Seconds(0.1), [&, R1R2Devices]() {
    Ptr<Queue> queue = StaticCast<PointToPointNetDevice>(R1R2Devices.Get(0))->GetQueue();
    queueSizeDataset.Add(Simulator::Now().GetSeconds(), queue->GetNPackets());
  });

  // Schedule the congestion window reduction every 10 seconds
  for (int i = 10; i < simulationTime; i += 10) {
    Simulator::Schedule(Seconds(i), []() {
      Config::Set("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/Socket/InitialCwnd",
                  UintegerValue(4)); // cwnd to 4 segments
      NS_LOG_INFO("Congestion window set to 4 segments at " << Simulator::Now().GetSeconds() << " s");
    });
  }

  // Add the datasets to the plots
  congestionWindowPlot.AddDataset(congestionWindowDataset);
  throughputPlot.AddDataset(throughputDataset);
  queueSizePlot.AddDataset(queueSizeDataset);

  // Run the simulation
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // Generate the plots
  congestionWindowPlot.GenerateOutput();
  throughputPlot.GenerateOutput();
  queueSizePlot.GenerateOutput();

  // Cleanup
  Simulator::Destroy();

  return 0;
}