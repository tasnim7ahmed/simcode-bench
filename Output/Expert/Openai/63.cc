#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    bool enableTracing = false;
    bool useNanosecondTimestamps = false;

    CommandLine cmd;
    cmd.AddValue("enableTracing", "Enable/disable packet capture tracing", enableTracing);
    cmd.AddValue("useNanosecondTimestamps", "Use nanosecond resolution timestamps in PCAP trace", useNanosecondTimestamps);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApp = bulkSendHelper.Install(nodes.Get(0));
    sourceApp.Start(Seconds(0.0));
    sourceApp.Stop(Seconds(10.0));

    if (enableTracing)
    {
        if (useNanosecondTimestamps)
        {
            pointToPoint.EnablePcap("tcp-pcap-nanosec-example", devices, true, true);
            Config::Set("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/EnablePcapNanosec", BooleanValue(true));
        }
        else
        {
            pointToPoint.EnablePcap("tcp-pcap-nanosec-example", devices, true, false);
        }
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}