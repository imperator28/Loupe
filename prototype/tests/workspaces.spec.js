import { expect, test } from "@playwright/test";

test("Inspect starts with the full assembly and viewport-anchored tool surface", async ({ page }) => {
  await page.goto("/");
  await expect(page.getByRole("tab", { name: "Inspect" })).toHaveAttribute("aria-selected", "true");
  await expect(page.locator(".brand-mark")).toHaveAttribute("aria-hidden", "true");
  await expect(page.locator(".topbar [role=tablist]")).toHaveCount(1);
  await expect(page.getByRole("button", { name: "Search commands" })).toHaveCount(0);
  await expect(page.locator(".tree-item.active")).toHaveCount(0);
  await expect(page.getByRole("region", { name: "Assembly viewer" })).toContainText("Full assembly");
  await expect(page.locator(".context-panel")).toContainText("Document properties");
  await expect(page.getByRole("button", { name: "Show properties" })).toHaveCount(0);
  await expect(page.locator(".master-view .inspection-dock")).toHaveCount(1);
  for (const name of ["Fit", "Isolate", "Section", "Measure", "Ghost", "Capture"]) {
    await expect(page.getByRole("button", { name })).toBeVisible();
    await expect(page.getByRole("button", { name }).locator("svg")).toHaveCount(1);
  }
});

test("source-unit control shows overall bounds and opens review", async ({ page }) => {
  await page.goto("/");
  const unitControl = page.getByRole("button", { name: /Review source units/ });
  await expect(unitControl).toContainText("238 × 148 × 62 mm");
  await unitControl.click();
  await expect(page.getByRole("dialog", { name: "Source unit review" })).toContainText("mm confirmed");
  await expect(page.getByRole("button", { name: "Close source unit review" })).toBeFocused();
  await page.keyboard.press("Shift+Tab");
  await expect(page.getByRole("button", { name: "Cancel unit review" })).toBeFocused();
  await page.keyboard.press("Tab");
  await expect(page.getByRole("button", { name: "Close source unit review" })).toBeFocused();
  await page.keyboard.press("Escape");
  await expect(page.getByRole("dialog", { name: "Source unit review" })).toHaveCount(0);
});

test("unit review applies a manual interpretation before export", async ({ page }) => {
  await page.goto("/?fixture=suspicious-units");
  await page.getByRole("button", { name: /Review source units/ }).click();
  await page.getByRole("combobox", { name: "Interpret source dimensions as" }).selectOption("in");
  await expect(page.getByRole("dialog", { name: "Source unit review" })).toContainText("9.37 × 5.83 × 2.44 in");
  await page.getByRole("button", { name: "Apply unit interpretation" }).click();
  await expect(page.getByRole("button", { name: /Review source units/ })).toContainText("in confirmed");
  await page.getByRole("tab", { name: "Export" }).click();
  await expect(page.getByRole("button", { name: "Review export plan" })).toBeEnabled();
});

test("highlight does not mutate the checked export set", async ({ page }) => {
  await page.goto("/");
  await page.getByRole("tab", { name: "Export" }).click();
  await page.getByRole("checkbox", { name: "Export Front cover" }).check();
  await page.getByRole("button", { name: /Lens guide/ }).click();
  await expect(page.getByRole("checkbox", { name: "Export Front cover" })).toBeChecked();
  await page.getByRole("tab", { name: "Inspect" }).click();
  await page.getByRole("tab", { name: "Export" }).click();
  await expect(page.getByRole("checkbox", { name: "Export Front cover" })).toBeChecked();
  await page.getByRole("button", { name: /Front cover/ }).click();
  await expect(page.getByTestId("master-highlight")).toHaveText("Front cover");
  await expect(page.getByTestId("isolated-preview")).toHaveText("Front cover");
  await expect(page.getByRole("checkbox", { name: /Export Front cover/ })).toBeChecked();
});

test("workspace tabs and component list support keyboard selection", async ({ page }) => {
  await page.goto("/");
  for (const tab of [page.getByRole("tab", { name: "Inspect" }), page.getByRole("tab", { name: "Export" })]) {
    const panelId = await tab.getAttribute("aria-controls");
    await expect(page.locator(`#${panelId}`)).toHaveCount(1);
  }
  const inspect = page.getByRole("tab", { name: "Inspect" });
  await inspect.focus();
  await page.keyboard.press("ArrowRight");
  await expect(page.getByRole("tab", { name: "Export" })).toBeFocused();
  await expect(page.getByRole("tab", { name: "Export" })).toHaveAttribute("aria-selected", "true");

  await expect(page.getByRole("listbox")).toHaveCount(0);
  const lensGuide = page.getByRole("button", { name: /Lens guide/ });
  await lensGuide.focus();
  await page.keyboard.press("Enter");
  await expect(page.getByTestId("master-highlight")).toHaveText("Lens guide");
});

test("Escape clears a component selection back to the full assembly", async ({ page }) => {
  await page.goto("/");
  await page.getByRole("button", { name: /Lens guide/ }).click();
  await expect(page.getByRole("region", { name: "Assembly viewer" })).toContainText("Selected: Lens guide");
  await page.keyboard.press("Escape");
  await expect(page.locator(".tree-item.active")).toHaveCount(0);
  await expect(page.getByRole("region", { name: "Assembly viewer" })).toContainText("Full assembly");
  await expect(page.locator(".context-panel")).toContainText("Document properties");
});

test("unresolved units block export review", async ({ page }) => {
  await page.goto("/?fixture=suspicious-units");
  await page.getByRole("tab", { name: "Export" }).click();
  await expect(page.getByRole("button", { name: "Review export plan" })).toBeDisabled();
});

test("keyboard focus remains visible and reduced motion does not remove state", async ({ page }) => {
  await page.emulateMedia({ reducedMotion: "reduce" });
  await page.goto("/");
  await page.getByRole("tab", { name: "Export" }).focus();
  await expect(page.getByRole("tab", { name: "Export" })).toBeFocused();
  await expect(page.getByRole("tab", { name: "Export" })).toHaveCSS("outline-width", "3px");
  await page.keyboard.press("Enter");
  await expect(page.getByRole("tab", { name: "Export" })).toHaveAttribute("aria-selected", "true");
  const duration = await page.getByRole("tab", { name: "Export" }).evaluate((element) => getComputedStyle(element).transitionDuration);
  expect(Number.parseFloat(duration)).toBeLessThanOrEqual(0.01);
});
