import Image from "next/image";

const controls = [
  ["D-PAD", "Move between jacks, crossings, bridge pegs, and City Memory."],
  ["A", "Pick up, patch, unplug, confirm, or flip the selected crossing."],
  ["B", "Hold to monitor a line. Tap to repeat the last clear phrase."],
  ["L", "Rehang the selected live cord over another brass bridge peg."],
  ["R", "Set the one ground clamp to suppress the next leak."],
  ["SELECT", "Open City Memory and inspect who knows each fact."],
  ["START", "Pause the shift and open accessibility options."],
] as const;

const evidence = [
  ["01", "NATIVE TARGET", "240 x 160 pixels"],
  ["02", "AUTHORED SHIFT", "Platform Three"],
  ["03", "LIVE SYSTEM", "4 calls / 5 facts"],
  ["04", "CONSEQUENCES", "4 objectives / 4 epilogues"],
] as const;

const consequenceFrames = [
  {
    number: "01",
    label: "THE BRIEF",
    title: "Four outcomes. No fail screen.",
    body: "Redirect the courier, stage three east-side cabs, protect a patient identity, and verify the platform through two sources.",
    src: "/screens/shift-brief.png",
    alt: "Platform Three shift brief listing four service, transport, privacy, and verification objectives.",
  },
  {
    number: "02",
    label: "THE MEMORY",
    title: "The city remembers what crosses.",
    body: "Every delivered or overheard fact changes City Memory: who knows it, how confident they are, and which callback may arrive next.",
    src: "/screens/city-memory.png",
    alt: "City Memory listing five facts with Platform 3, not 8 selected and known by no one yet.",
  },
  {
    number: "03",
    label: "THE DAWN",
    title: "The ending reads your topology.",
    body: "A missed connection, a useful leak, or one unguarded secret persists into the shift report, playback timeline, and dawn epilogue.",
    src: "/screens/epilogue.png",
    alt: "A Port Meridian dawn epilogue in which a sealed medical case waits beside a freight platform.",
  },
] as const;

export default function Home() {
  return (
    <main>
      <header className="masthead">
        <a className="studio-mark" href="#top" aria-label="LINE NOISE home">
          <span className="lamp" aria-hidden="true" />
          UPPERCUT LABS / CENTRAL EXCHANGE
        </a>
        <nav aria-label="Primary navigation">
          <a href="#signal">Signal</a>
          <a href="#consequences">Consequences</a>
          <a href="#controls">Controls</a>
          <a href="#proof">Build proof</a>
        </nav>
      </header>

      <section className="hero" id="top" aria-labelledby="game-title">
        <div className="hero-copy">
          <p className="eyebrow">
            <span className="status-dot" aria-hidden="true" />
            PLAYABLE GAME BOY ADVANCE VERTICAL SLICE
          </p>
          <h1 id="game-title">
            <span>LINE</span>
            <span>
              NOISE<span className="title-bang">!</span>
            </span>
          </h1>
          <p className="deck">
            Patch calls across Port Meridian&apos;s old manual exchange. Monitor
            live lines, shape where cords cross, and decide which facts leak -
            and which stay private.
          </p>
          <div className="hero-actions">
            <a
              className="button button-primary"
              href="/downloads/line-noise.gba"
              download="line-noise.gba"
            >
              DOWNLOAD .GBA
              <span aria-hidden="true">&darr;</span>
            </a>
            <a className="button button-quiet" href="#controls">
              READ THE BOARD
            </a>
          </div>
        </div>

        <figure className="hero-artifact">
          <div className="screen-housing">
            <span className="screw screw-tl" aria-hidden="true" />
            <span className="screw screw-tr" aria-hidden="true" />
            <span className="screw screw-bl" aria-hidden="true" />
            <span className="screw screw-br" aria-hidden="true" />
            <div className="screen-label">
              <span>PORT MERIDIAN</span>
              <span>23:47 / LIVE</span>
            </div>
            <Image
              className="pixel-screen"
              src="/screens/four-live-cords.png"
              alt="Native GBA gameplay with Mara, Bea, Cal, and Felix patched across four colored, crossing cords beside the City Memory display."
              width={240}
              height={160}
              priority
              unoptimized
            />
            <figcaption>
              Native capture / four live calls / three cord crossings
            </figcaption>
          </div>
          <div className="cord cord-cyan" aria-hidden="true" />
          <div className="cord cord-coral" aria-hidden="true" />
        </figure>

        <div className="evidence-rail" aria-label="Game facts">
          {evidence.map(([number, label, value]) => (
            <div className="evidence-item" key={number}>
              <span>{number}</span>
              <p>
                <strong>{label}</strong>
                {value}
              </p>
            </div>
          ))}
        </div>
      </section>

      <section className="signal-section" id="signal" aria-labelledby="signal-title">
        <div className="section-copy">
          <p className="section-kicker">THE DEFINING RULE</p>
          <h2 id="signal-title">
            Connections carry people.
            <br />
            <em>Crossings carry information.</em>
          </h2>
          <p>
            A correct patch is only the start. Rehang a cord, flip which line
            sits on top, or set a ground clamp. Facts copied between calls can
            change who acts next, what City Memory records, and how dawn reads.
          </p>
          <ol className="verb-strip" aria-label="Core game loop">
            <li><span>01</span> PATCH</li>
            <li><span>02</span> SHAPE</li>
            <li><span>03</span> LIVE WITH IT</li>
          </ol>
        </div>

        <figure className="diagram-frame">
          <div className="diagram-tag">DESIGN DIAGRAM / NOT GAMEPLAY</div>
          <Image
            src="/design/mechanic-diagram.png"
            alt="Three-step design diagram: patch calls, leak Platform 3 at a crossing, then show Owen learning the fact and calling Rail."
            width={960}
            height={600}
            unoptimized
          />
          <figcaption>
            Patch Mara to Owen. Let Platform 3 cross into Cal&apos;s line. Watch
            that new knowledge become the next action.
          </figcaption>
        </figure>
      </section>

      <section className="consequence-section" id="consequences" aria-labelledby="consequence-title">
        <div className="section-heading">
          <p className="section-kicker">ONE SHIFT / MANY CITY STATES</p>
          <h2 id="consequence-title">The board is a cause-and-effect machine.</h2>
          <p>Brief, memory, and dawn are three views of the same live system.</p>
        </div>
        <div className="consequence-track">
          {consequenceFrames.map((frame) => (
            <article className="consequence-card" key={frame.number}>
              <div className="frame-index">
                <span>{frame.number}</span>
                {frame.label}
              </div>
              <div className="capture-frame">
                <Image
                  src={frame.src}
                  alt={frame.alt}
                  width={240}
                  height={160}
                  unoptimized
                />
              </div>
              <h3>{frame.title}</h3>
              <p>{frame.body}</p>
            </article>
          ))}
        </div>
      </section>

      <section className="controls-section" id="controls" aria-labelledby="controls-title">
        <div className="controls-intro">
          <p className="section-kicker">OPERATOR&apos;S DESK</p>
          <h2 id="controls-title">Seven inputs. One city listening.</h2>
          <p>
            The first call can be handled with the D-pad, A, and B. Pegs,
            crossing order, and the single-use clamp deepen the same tactile
            board instead of turning it into freehand routing.
          </p>
        </div>
        <dl className="control-board">
          {controls.map(([input, action]) => (
            <div className="control-row" key={input}>
              <dt>{input}</dt>
              <dd>{action}</dd>
            </div>
          ))}
        </dl>
      </section>

      <section className="proof-section" id="proof" aria-labelledby="proof-title">
        <div className="proof-title-capture">
          <div className="mini-screen">
            <Image
              src="/screens/title.png"
              alt="LINE NOISE! title screen, with a cyan live cord cutting through the cream logo."
              width={240}
              height={160}
              unoptimized
            />
          </div>
          <p>THE EXCLAMATION POINT IS IN THE CARTRIDGE.</p>
        </div>
        <div className="proof-copy">
          <p className="section-kicker">VALIDATED BUILD / 14 AUG 2026</p>
          <h2 id="proof-title">A real ROM, not a target render.</h2>
          <p>
            The downloadable cartridge is built with devkitARM and verified by
            a frame-exact mGBA route. The supplied source art separately passes
            its complete isolated validation suite.
          </p>
          <div className="proof-grid">
            <div><strong>22 / 22</strong><span>host + verifier tests</span></div>
            <div><strong>10</strong><span>decoded native checkpoints</span></div>
            <div><strong>956 / 956</strong><span>source-asset checks</span></div>
            <div><strong>188.4 KiB</strong><span>final ROM size</span></div>
          </div>
          <p className="checksum">
            SHA-256 <code>ca365eb66dd22f720063fc3b58be07ba6e07e2b52eea0cea3592850dc2420f42</code>
          </p>
          <a
            className="button button-primary"
            href="/downloads/line-noise.gba"
            download="line-noise.gba"
          >
            TAKE THE NIGHT SHIFT
            <span aria-hidden="true">&rarr;</span>
          </a>
        </div>
      </section>

      <footer>
        <Image
          src="/brand/uppercut-labs-logo.png"
          alt="Uppercut Labs"
          width={128}
          height={32}
          unoptimized
        />
        <p>
          Developed by Uppercut Labs. Playable GBA vertical slice built from
          the supplied original concept and asset pack.
        </p>
        <a href="#top">RETURN TO THE JACKS &uarr;</a>
      </footer>
    </main>
  );
}
