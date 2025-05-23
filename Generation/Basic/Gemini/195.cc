#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/bulk-send-helper.h"

#include <iostream>
#include <string>
#include <iomanip>

using namespace ns3;

int main(int argc, char *argv[])
{
    double simulationTime = 10.0; // seconds
    std::string dataRate = "10Mbps";
    std::string delay = "10ms";
    uint16_t port = 9; // Discard port for PacketSink

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("dataRate", "Data rate of the point-to-point link", dataRate);
    cmd.AddValue("delay", "Delay of the point-to-point link", delay);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2pHelper.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer devices;
    devices = p2pHelper.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Server application: PacketSink
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer serverApps = packetSinkHelper.Install(nodes.Get(1)); // Server is node 1
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime + 1.0)); 

    // Client application: BulkSend
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port)); // Connect to server's IP address
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // 0 means send until application stops
    ApplicationContainer clientApps = bulkSendHelper.Install(nodes.Get(0)); // Client is node 0
    clientApps.Start(Seconds(1.0)); 
    clientApps.Stop(Seconds(simulationTime)); 

    p2pHelper.EnablePcapAll("tcp-bulk-transfer");

    Simulator::Stop(Seconds(simulationTime + 2.0)); 
    Simulator::Run();

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApps.Get(0));
    uint64_t totalRxBytes = sink->GetTotalRx();

    Time clientStartTime = clientApps.Get(0)->GetStartTime();
    Time clientStopTime = clientApps.Get(0)->GetStopTime();
    double dataTransferDuration = (clientStopTime - clientStartTime).GetSeconds();

    std::cout << "Simulation finished." << std::endl;
    std::cout << "Data Rate: " << dataRate << ", Delay: " << delay << std::endl;
    std::cout << "Total Bytes Received: " << totalRxBytes << " bytes" << std::endl;

    if (dataTransferDuration > 0)
    {
        double throughputMbps = (totalRxBytes * 8.0) / (dataTransferDuration * 1000000.0);
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Throughput: " << throughputMbps << " Mbps" << std::endl;
    }
    else
    {
        std::cout << "Throughput: 0.000 Mbps (Data transfer duration was zero or negative)" << std::endl;
    }

    Simulator::Destroy();

    return 0;
}