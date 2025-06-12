#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/propagation-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInfraFixedRssExample");

int
main(int argc, char *argv[])
{
    double rss = -80.0;
    uint32_t packetSize = 1024;
    uint32_t numPackets = 10;
    double interval = 1.0;
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("rss", "Fixed RSS (dBm) received by access point", rss);
    cmd.AddValue("packetSize", "Size of each packet sent (bytes)", packetSize);
    cmd.AddValue("numPackets", "Total number of packets to send", numPackets);
    cmd.AddValue("interval", "Interval (seconds) between packets", interval);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("WifiInfraFixedRssExample", LOG_LEVEL_INFO);
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("YansWifiPhy", LOG_LEVEL_INFO);
    }

    NodeContainer wifiStaNode, wifiApNode;
    wifiStaNode.Create(1);
    wifiApNode.Create(1);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    Ptr<FixedRssLossModel> lossModel = CreateObject<FixedRssLossModel>();
    lossModel->SetRss(rss);
    wifiChannel.SetPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(rss));
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(wifiChannel.Create());
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("DsssRate11Mbps"),
                                "ControlMode", StringValue("DsssRate1Mbps"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi-fixedrss");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // STA
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevice);

    UdpServerHelper server(4000);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(20.0));

    UdpClientHelper client(apInterfaces.GetAddress(0), 4000);
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(20.0));

    phy.EnablePcap("wifi-fixedrss-sta", staDevice.Get(0));
    phy.EnablePcap("wifi-fixedrss-ap", apDevice.Get(0));

    Simulator::Stop(Seconds(21.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}