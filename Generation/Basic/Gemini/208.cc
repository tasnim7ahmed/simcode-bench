#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <iomanip> // For std::fixed and std::setprecision

// Global counters for packet loss and handovers
static uint64_t g_totalTxPackets = 0;
static uint64_t g_totalRxPackets = 0;
static uint32_t g_handoverCount = 0;

// Trace callback for UDP client Tx
void PacketTxTrace(Ptr<const Packet> p, Ptr<const Application> app)
{
    g_totalTxPackets++;
}

// Trace callback for PacketSink Rx
void PacketRxTrace(Ptr<const Packet> p, Ptr<const Application> app)
{
    g_totalRxPackets++;
}

namespace ns3 {

/**
 * \brief Custom WifiHandoverManagement implementation for signal strength-based handover.
 *
 * This class monitors beacon Rx power from available APs and triggers a handover
 * when a different AP offers a significantly stronger signal than the currently
 * associated one, or when the current association's signal becomes very weak.
 */
class MyWifiHandoverManagement : public WifiHandoverManagement
{
public:
    static TypeId GetTypeId();

    /**
     * \brief Constructor for MyWifiHandoverManagement.
     * \param commonSsid The common SSID used by all APs in the network.
     * \param thresholdDbm The minimum dBm difference required for a handover to be triggered.
     *                     A positive value means the new AP must be stronger by this amount.
     */
    MyWifiHandoverManagement(Ssid commonSsid, double thresholdDbm);

    /**
     * \brief Destructor.
     */
    ~MyWifiHandoverManagement() override;

    /**
     * \brief Sets the StaWifiMac instance that this manager will control.
     * \param mac The StaWifiMac instance.
     */
    void SetStaWifiMac(Ptr<StaWifiMac> mac);

private:
    void DoInitialize() override;
    void DoDispose() override;

    /**
     * \brief Callback for received beacons.
     * Stores the Rx power for each observed AP.
     * \param hdr The MAC header of the received beacon.
     * \param rxPowerDbm The received signal strength in dBm.
     * \param rxDuration The duration of the received signal.
     * \param rxPhy The WifiPhy instance that received the beacon.
     */
    void DoNotifyBeaconRx(const WifiMacHeader& hdr, double rxPowerDbm, Time rxDuration, WifiPhy* rxPhy) override;

    /**
     * \brief Callback for successful new association.
     * Updates the currently associated AP.
     * \param bssid The BSSID of the newly associated AP.
     * \param ssid The SSID of the newly associated AP.
     */
    void DoNotifyNewAssociatedAp(const Mac48Address& bssid, const Ssid& ssid) override;

    /**
     * \brief Callback for lost association.
     * Clears the current association status.
     * \param bssid The BSSID of the AP whose association was lost.
     */
    void DoNotifyLostAssociation(const Mac48Address& bssid) override;

    /**
     * \brief Called by StaWifiMac to request a handover decision.
     * Determines if a handover is necessary based on signal strengths.
     * \param bssid The BSSID of the AP to handover to (output parameter).
     * \param ssid The SSID of the AP to handover to (output parameter).
     * \return True if a handover is requested, false otherwise.
     */
    bool DoHandoverRequested(Mac48Address& bssid, Ssid& ssid) override;

    /**
     * \brief Helper to find the AP with the strongest signal among observed APs.
     * \return The BSSID of the best observed AP, or BroadcastMac48Address if none found.
     */
    Mac48Address FindBestAp() const;

    Ptr<StaWifiMac> m_staMac;                   ///< The StaWifiMac instance managed by this object
    std::map<Mac48Address, double> m_apRxPowers; ///< Map of BSSID to latest observed Rx power
    Mac48Address m_currentApBssid;              ///< BSSID of the currently associated AP
    Ssid m_currentApSsid;                       ///< SSID of the currently associated AP
    Ssid m_commonSsid;                          ///< The common SSID used by all APs
    double m_handoverThresholdDbm;              ///< Signal strength difference threshold for handover
};

NS_OBJECT_ENSURE_REGISTERED(MyWifiHandoverManagement);

TypeId MyWifiHandoverManagement::GetTypeId()
{
    static TypeId tid = TypeId("ns3::MyWifiHandoverManagement")
        .SetParent<WifiHandoverManagement>()
        .SetGroupName("Wifi")
        .AddConstructor<MyWifiHandoverManagement>()
        .AddAttribute("CommonSsid",
                      "The common SSID used by all access points.",
                      SsidValue(Ssid()),
                      MakeSsidAccessor(&MyWifiHandoverManagement::m_commonSsid),
                      MakeSsidChecker())
        .AddAttribute("HandoverThresholdDbm",
                      "The minimum dBm difference for a handover to be triggered.",
                      DoubleValue(0.0),
                      MakeDoubleAccessor(&MyWifiHandoverManagement::m_handoverThresholdDbm),
                      MakeDoubleChecker<double>());
    return tid;
}

MyWifiHandoverManagement::MyWifiHandoverManagement(Ssid commonSsid, double thresholdDbm)
    : m_staMac(nullptr),
      m_commonSsid(commonSsid),
      m_handoverThresholdDbm(thresholdDbm)
{
}

MyWifiHandoverManagement::~MyWifiHandoverManagement()
{
}

void MyWifiHandoverManagement::SetStaWifiMac(Ptr<StaWifiMac> mac)
{
    m_staMac = mac;
}

void MyWifiHandoverManagement::DoInitialize()
{
    WifiHandoverManagement::DoInitialize();
    m_apRxPowers.clear();
    m_currentApBssid = Mac48Address(); // Invalid address initially
    m_currentApSsid = Ssid();         // Empty SSID initially
}

void MyWifiHandoverManagement::DoDispose()
{
    m_staMac = nullptr;
    m_apRxPowers.clear();
    WifiHandoverManagement::DoDispose();
}

void MyWifiHandoverManagement::DoNotifyBeaconRx(const WifiMacHeader& hdr, double rxPowerDbm, Time rxDuration, WifiPhy* rxPhy)
{
    // Update the observed Rx power for this AP if it matches our common SSID
    if (hdr.GetSsid() == m_commonSsid)
    {
        m_apRxPowers[hdr.GetAddr2()] = rxPowerDbm;
    }
}

void MyWifiHandoverManagement::DoNotifyNewAssociatedAp(const Mac48Address& bssid, const Ssid& ssid)
{
    if (m_currentApBssid.IsBroadcast()) // Initial association
    {
        NS_LOG_INFO("STA " << m_staMac->GetAddress() << " initially associated with AP " << bssid << " (SSID: " << ssid << ")");
    }
    else if (m_currentApBssid != bssid) // Handover occurred
    {
        g_handoverCount++;
        NS_LOG_INFO("STA " << m_staMac->GetAddress() << " handed over from AP " << m_currentApBssid << " to AP " << bssid << " (SSID: " << ssid << ")");
    }
    m_currentApBssid = bssid;
    m_currentApSsid = ssid;
}

void MyWifiHandoverManagement::DoNotifyLostAssociation(const Mac48Address& bssid)
{
    NS_LOG_INFO("STA " << m_staMac->GetAddress() << " lost association with AP " << bssid);
    m_currentApBssid = Mac48Address(); // Invalidate current AP
    m_currentApSsid = Ssid();
}

Mac48Address MyWifiHandoverManagement::FindBestAp() const
{
    Mac48Address bestAp = Mac48Address();
    double bestRxPower = -1000.0; // Initialize with a very small number

    for (auto const& [bssid, rxPower] : m_apRxPowers)
    {
        if (rxPower > bestRxPower)
        {
            bestRxPower = rxPower;
            bestAp = bssid;
        }
    }
    return bestAp;
}

bool MyWifiHandoverManagement::DoHandoverRequested(Mac48Address& bssid, Ssid& ssid)
{
    // If not currently associated or lost association, try to associate with the best AP
    if (m_currentApBssid.IsBroadcast())
    {
        Mac48Address candidateAp = FindBestAp();
        if (!candidateAp.IsBroadcast())
        {
            bssid = candidateAp;
            ssid = m_commonSsid;
            NS_LOG_INFO("STA " << m_staMac->GetAddress() << ": Initial association requested with AP " << bssid << " (SSID: " << ssid << ")");
            return true;
        }
        return false;
    }

    // Determine current AP's signal strength
    double currentApRxPower = -1000.0; // Default if current AP's beacon not recently seen
    if (m_apRxPowers.count(m_currentApBssid))
    {
        currentApRxPower = m_apRxPowers[m_currentApBssid];
    }
    else
    {
        // If current AP's beacon is not in our recent observations, its signal might be lost or very weak.
        // This acts as a strong incentive to look for other APs.
        currentApRxPower = -200.0; // Simulate very weak signal
    }

    // Find the best alternative AP
    Mac48Address candidateAp = Mac48Address();
    double candidateRxPower = -1000.0;

    for (auto const& [ap_bssid, ap_rxPower] : m_apRxPowers)
    {
        if (ap_bssid == m_currentApBssid) continue; // Don't consider current AP as a candidate for handover

        if (ap_rxPower > candidateRxPower)
        {
            candidateRxPower = ap_rxPower;
            candidateAp = ap_bssid;
        }
    }

    // Handover decision logic:
    // 1. If a candidate AP exists AND it is significantly stronger than the current AP
    if (!candidateAp.IsBroadcast() && (candidateRxPower - currentApRxPower > m_handoverThresholdDbm))
    {
        NS_LOG_INFO("STA " << m_staMac->GetAddress() << ": Considering handover from AP " << m_currentApBssid << " (" << currentApRxPower << " dBm) to AP " << candidateAp << " (" << candidateRxPower << " dBm). Threshold: " << m_handoverThresholdDbm << " dBm.");
        bssid = candidateAp;
        ssid = m_commonSsid;
        return true;
    }
    // 2. If current AP's signal is very poor and there are other APs available (even if not much stronger)
    else if (currentApRxPower < -90.0 && !m_apRxPowers.empty())
    {
        Mac48Address anyAp = FindBestAp();
        if (!anyAp.IsBroadcast() && anyAp != m_currentApBssid)
        {
            NS_LOG_INFO("STA " << m_staMac->GetAddress() << ": Current AP signal very low (" << currentApRxPower << " dBm), forcing re-association to best available AP " << anyAp << " (" << m_apRxPowers[anyAp] << " dBm).");
            bssid = anyAp;
            ssid = m_commonSsid;
            return true;
        }
    }

    return false;
}

} // namespace ns3

int main(int argc, char *argv[])
{
    // Set up logging
    LogComponentEnable("MyWifiHandoverManagement", LOG_LEVEL_INFO);
    LogComponentEnable("StaWifiMac", LOG_LEVEL_INFO);
    LogComponentEnable("ApWifiMac", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpClient", LOG_LEVEL_WARN); // Reduce verbose client logs
    // LogComponentEnable("PacketSink", LOG_LEVEL_WARN); // Reduce verbose sink logs
    // LogComponentEnable("WifiPhy", LOG_LEVEL_DEBUG); // Enable for detailed Rx signal reports (very verbose)

    // Simulation parameters
    double simulationTime = 120.0;     // seconds
    uint32_t numStaNodes = 6;
    double areaX = 200.0;              // X dimension of mobility area
    double areaY = 100.0;              // Y dimension of mobility area
    double ap1X = 0.0;                 // AP1 X position
    double ap1Y = 0.0;                 // AP1 Y position
    double ap2X = 100.0;               // AP2 X position
    double ap2Y = 0.0;                 // AP2 Y position
    double staSpeed = 1.0;             // Speed of STA nodes in m/s
    double staPause = 0.5;             // Pause duration for STA nodes in seconds
    double handoverThreshold = 5.0;    // Signal strength difference (dBm) to trigger handover

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("numStaNodes", "Number of STA nodes", numStaNodes);
    cmd.AddValue("areaX", "X dimension of mobility area", areaX);
    cmd.AddValue("areaY", "Y dimension of mobility area", areaY);
    cmd.AddValue("staSpeed", "Speed of STA nodes in m/s", staSpeed);
    cmd.AddValue("handoverThreshold", "Handover trigger threshold in dBm", handoverThreshold);
    cmd.Parse(argc, argv);

    // 1. Create Nodes
    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes;
    staNodes.Create(numStaNodes);

    // Combine all nodes for NetAnim visualization
    NodeContainer allNodes = apNodes;
    allNodes.Add(staNodes);

    // 2. Setup Mobility
    // APs at fixed positions
    MobilityHelper mobilityAp;
    Ptr<ListPositionAllocator> positionAllocAp = CreateObject<ListPositionAllocator>();
    positionAllocAp->Add(Vector(ap1X, ap1Y, 0.0)); // AP1 position
    positionAllocAp->Add(Vector(ap2X, ap2Y, 0.0)); // AP2 position
    mobilityAp.SetPositionAllocator(positionAllocAp);
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(apNodes);

    // STAs with RandomWalk2dMobilityModel
    MobilityHelper mobilitySta;
    // Define the movement area to cover both APs and surrounding space
    mobilitySta.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(-areaX / 2.0, areaX / 2.0 + (ap2X - ap1X), -areaY / 2.0, areaY / 2.0)),
                                 "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(staSpeed) + "]"),
                                 "Direction", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=6.283185]"), // 0 to 2*PI radians
                                 "Pause", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(staPause) + "]"));
    mobilitySta.Install(staNodes);

    // 3. Setup Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n); // Use 802.11n standard

    YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    channel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiMacHelper mac;
    Ssid commonSsid = Ssid("MyAP_SSID"); // All APs will use the same SSID for seamless handover

    // Configure AP Macs
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(commonSsid),
                "BeaconInterval", TimeValue(MicroSeconds(102400))); // Default is 102400 us = 102.4 ms
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, apNodes);

    // Configure STA Macs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(commonSsid),
                "ActiveProbing", BooleanValue(false)); // Don't actively probe, rely on beacons
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, staNodes);

    // Set custom handover management for each STA
    for (uint32_t i = 0; i < numStaNodes; ++i)
    {
        Ptr<StaWifiMac> staMac = DynamicCast<StaWifiMac>(staDevices.Get(i)->GetMac());
        NS_ASSERT_MSG(staMac != nullptr, "STA device does not have a StaWifiMac!");
        Ptr<ns3::MyWifiHandoverManagement> myHandoverMgmt = CreateObject<ns3::MyWifiHandoverManagement>(commonSsid, handoverThreshold);
        myHandoverMgmt->SetStaWifiMac(staMac);
        staMac->SetHandoverManagement(myHandoverMgmt);
    }

    // 4. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces;
    apInterfaces = address.Assign(apDevices); // APs get IPs in 10.0.0.x
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices); // STAs get IPs in 10.0.0.x

    // 5. Install Applications (UDP traffic from STAs to AP1)
    // AP1 as server
    Ptr<Node> serverNode = apNodes.Get(0); // AP1 hosts the server
    uint16_t port = 9; // Discard port

    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = packetSinkHelper.Install(serverNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime + 1.0));

    // Connect PacketSink Rx trace
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApps.Get(0));
    if (sink) {
        sink->TraceConnectWithoutContext("Rx", MakeCallback(&PacketRxTrace));
    }

    // STAs as clients
    // Target AP1's IP address (the server)
    UdpClientHelper udpClientHelper(apInterfaces.GetAddress(0), port);
    udpClientHelper.SetAttribute("MaxPackets", UintegerValue(4294967295U)); // Send a very large number of packets
    udpClientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(100))); // Send a packet every 100ms
    udpClientHelper.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes per packet

    ApplicationContainer clientApps = udpClientHelper.Install(staNodes);
    clientApps.Start(Seconds(1.0)); // Start client traffic after 1 second
    clientApps.Stop(Seconds(simulationTime));

    // Connect UdpClient Tx trace
    for (uint32_t i = 0; i < numStaNodes; ++i)
    {
        Ptr<UdpClient> client = DynamicCast<UdpClient>(clientApps.Get(i));
        if (client) {
            client->TraceConnectWithoutContext("Tx", MakeCallback(&PacketTxTrace));
        }
    }

    // Enable NetAnim visualization
    NetAnimHelper anim;
    anim.EnableAnimation("wifi-handover-animation.xml", allNodes);

    // Run simulation
    Simulator::Stop(Seconds(simulationTime + 2.0)); // Give some extra time for final events/cleanup
    Simulator::Run();

    // Print results
    NS_LOG_UNCOND("Simulation finished.");
    NS_LOG_UNCOND("Total Handover Events: " << g_handoverCount);
    NS_LOG_UNCOND("Total Packets Sent: " << g_totalTxPackets);
    NS_LOG_UNCOND("Total Packets Received: " << g_totalRxPackets);
    if (g_totalTxPackets > 0)
    {
        double packetLossRate = (double)(g_totalTxPackets - g_totalRxPackets) / g_totalTxPackets * 100.0;
        NS_LOG_UNCOND("Packet Loss Rate: " << std::fixed << std::setprecision(2) << packetLossRate << "%");
    }
    else
    {
        NS_LOG_UNCOND("No packets sent.");
    }

    Simulator::Destroy();

    return 0;
}