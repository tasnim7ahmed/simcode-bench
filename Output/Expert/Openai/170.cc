#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

uint32_t g_rxPackets = 0;
uint32_t g_rxBytes = 0;

void RxPacketCounter(Ptr<const Packet> packet, const Address &address)
{
    g_rxPackets++;
    g_rxBytes += packet->GetSize();
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("DsssRate11Mbps"),
                                "ControlMode", StringValue("DsssRate11Mbps"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(5.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    uint16_t port = 50000;

    Address serverAddress(InetSocketAddress(staInterfaces.GetAddress(0), port));

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(wifiStaNodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    sink->TraceConnectWithoutContext("Rx", MakeCallback(&RxPacketCounter));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("11Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1400));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer clientApp = clientHelper.Install(wifiStaNodes.Get(1));

    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("ap", apDevice.Get(0));
    phy.EnablePcap("sta1", staDevices.Get(0));
    phy.EnablePcap("sta2", staDevices.Get(1));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    double throughput = (g_rxBytes * 8.0) / 10.0 / 1e6;

    std::cout << "Server received packets: " << g_rxPackets << std::endl;
    std::cout << "Server received bytes:   " << g_rxBytes << std::endl;
    std::cout << "Throughput at server:    " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}