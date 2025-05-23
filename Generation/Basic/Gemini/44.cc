#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/fixed-rss-loss-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiFixedRssExample");

int main(int argc, char *argv[])
{
    double rss = -60.0; 
    uint32_t packetSize = 1024; 
    uint32_t numPackets = 10;
    double intervalSeconds = 1.0; 
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("rss", "Received Signal Strength (dBm)", rss);
    cmd.AddValue("packetSize", "Size of packets (bytes)", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("interval", "Interval between packets (seconds)", intervalSeconds);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.Parse(argc, argv);

    Time interval = Seconds(intervalSeconds);

    if (verbose)
    {
        LogComponentEnable("WifiFixedRssExample", LOG_LEVEL_INFO);
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
        LogComponentEnable("WifiMac", LOG_LEVEL_INFO);
        LogComponentEnable("YansWifiPhy", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2); 

    Ptr<Node> apNode = nodes.Get(0);
    Ptr<Node> staNode = nodes.Get(1);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    apNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    staNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(1.0, 0.0, 0.0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel;
    Ptr<FixedRssLossModel> lossModel = CreateObject<FixedRssLossModel>();
    lossModel->SetAttribute("Rss", DoubleValue(rss));
    channel.SetPropagationLossModel(lossModel);
    channel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.SetErrorRateModel("ns3::YansErrorRateModel"); 
    phy.Set("TxPowerStart", DoubleValue(15.0));
    phy.Set("TxPowerEnd", DoubleValue(15.0));

    WifiMacHelper mac;
    NetDeviceContainer apDevice, staDevice;

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("ns-3-ssid")),
                "BeaconInterval", TimeValue(MicroSeconds(102400)));

    apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("ns-3-ssid")),
                "ActiveProbing", BooleanValue(false));
    staDevice = wifi.Install(phy, mac, staNode);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(apDevice);
    interfaces = address.Assign(staDevice);

    uint16_t port = 9; 

    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(apNode);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(numPackets * intervalSeconds + 1.0));

    OnOffHelper onoffHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port)); 
    onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(intervalSeconds) + "]"));
    onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); 
    onoffHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoffHelper.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets)); 

    ApplicationContainer clientApp = onoffHelper.Install(staNode);
    clientApp.Start(Seconds(1.0)); 
    clientApp.Stop(Seconds(numPackets * intervalSeconds + 1.0)); 

    phy.EnablePcap("wifi-fixed-rss-ap", apDevice.Get(0));
    phy.EnablePcap("wifi-fixed-rss-sta", staDevice.Get(0));

    Simulator::Stop(Seconds(numPackets * intervalSeconds + 2.0)); 
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}