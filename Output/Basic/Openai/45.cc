#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInterferenceExample");

int main(int argc, char *argv[])
{
    double primaryRssDbm = -80.0;
    double interfererRssDbm = -80.0;
    double interfererOffset = 0.0;
    uint32_t primaryPktSize = 1000;
    uint32_t interfererPktSize = 1000;
    bool verbose = false;
    std::string pcapPrefix = "wifi-interference";

    CommandLine cmd;
    cmd.AddValue("primaryRssDbm", "Target RSS at receiver for primary TX [dBm]", primaryRssDbm);
    cmd.AddValue("interfererRssDbm", "Target RSS at receiver for interferer [dBm]", interfererRssDbm);
    cmd.AddValue("offset", "Time offset (seconds) for interferer w.r.t primary TX", interfererOffset);
    cmd.AddValue("primaryPktSize", "Primary packet size (bytes)", primaryPktSize);
    cmd.AddValue("interfererPktSize", "Interferer packet size (bytes)", interfererPktSize);
    cmd.AddValue("verbose", "Enable verbose Wi-Fi logging", verbose);
    cmd.AddValue("pcapPrefix", "Prefix for pcap trace file", pcapPrefix);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        WifiHelper::EnableLogComponents();
        LogComponentEnable("Txop", LOG_LEVEL_ALL);
        LogComponentEnable("InterferenceHelper", LOG_LEVEL_ALL);
        LogComponentEnable("YansWifiPhy", LOG_LEVEL_ALL);
    }

    NodeContainer nodes;
    nodes.Create(3); // 0: TX, 1: RX, 2: Interferer

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    Vector rxPos(0.0, 0.0, 0.0);
    Vector txPos(5.0, 0.0, 0.0);
    Vector interfererPos(-5.0, 0.0, 0.0);
    positionAlloc->Add(txPos);         // TX
    positionAlloc->Add(rxPos);         // RX
    positionAlloc->Add(interfererPos); // Interferer
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    double freq = 2.412e9; // 802.11b Channel 1
    double c = 3e8;
    double lambda = c / freq;

    auto dbmToW = [](double dbm) { return std::pow(10.0, dbm / 10.0) / 1000.0; };
    auto wToDbm = [](double w) { return 10.0 * std::log10(w * 1000.0); };

    // Calculate distances from TX/interferer to RX
    Ptr<MobilityModel> rxMob = nodes.Get(1)->GetObject<MobilityModel>();
    Ptr<MobilityModel> txMob = nodes.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> interfMob = nodes.Get(2)->GetObject<MobilityModel>();
    double txRxDist = txMob->GetDistanceFrom(rxMob);
    double interfRxDist = interfMob->GetDistanceFrom(rxMob);

    // Calculate required TX powers to produce requested RSS at RX
    auto requiredTxPower = [lambda](double dist, double rssDbm) {
        // Friis: PrxdBm = PtxdBm + 20*log10(lambda/(4pi*d))
        double lossDb = 20.0 * std::log10(lambda / (4 * M_PI * dist));
        return rssDbm - lossDb;
    };
    double txPowerDbm = requiredTxPower(txRxDist, primaryRssDbm);
    double interfererPowerDbm = requiredTxPower(interfRxDist, interfererRssDbm);

    Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(txPowerDbm));
    Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(txPowerDbm));
    Config::Set("/NodeList/2/DeviceList/0/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(interfererPowerDbm));
    Config::Set("/NodeList/2/DeviceList/0/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(interfererPowerDbm));

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign(devices);

    uint16_t port = 12345;

    // Application for receiving on RX
    UdpServerHelper server(port);
    ApplicationContainer apps = server.Install(nodes.Get(1));
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(10.0));

    // Standard Tx: TX -> RX
    UdpClientHelper client(ifaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(primaryPktSize));
    ApplicationContainer txApp = client.Install(nodes.Get(0));
    txApp.Start(Seconds(1.0));
    txApp.Stop(Seconds(2.0));

    // Interferer Tx: Force tx with no DCF (AWGN)
    // Create a custom OnOff app with constant 'on' time, disable carrier sense using ConfigureStandard/Phy attributes
    Ptr<WifiNetDevice> interfererWifi = DynamicCast<WifiNetDevice>(devices.Get(2));
    interfererWifi->GetPhy()->SetAttribute("CcaMode1Threshold", DoubleValue(-1e9)); // Always transmit

    OnOffHelper interferer("ns3::UdpSocketFactory", InetSocketAddress(ifaces.GetAddress(1), port));
    interferer.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.1]"));
    interferer.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    interferer.SetAttribute("DataRate", StringValue("54Mbps"));
    interferer.SetAttribute("PacketSize", UintegerValue(interfererPktSize));
    ApplicationContainer interfApp = interferer.Install(nodes.Get(2));
    interfApp.Start(Seconds(1.0 + interfererOffset));
    interfApp.Stop(Seconds(2.0 + interfererOffset));

    wifiPhy.EnablePcap(pcapPrefix, devices, true);

    Simulator::Stop(Seconds(3.0 + interfererOffset));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}